#!/usr/bin/python

from __future__ import division
import PIL
import PIL.Image
import itertools
import math
import sys
import numpy
from scipy import signal
from scipy import ndimage

MAX_PIXEL = 255

########################################

def read_image(image_file):
    image = PIL.Image.open(image_file)
    try:
        data = image.getdata()
    except ValueError:
        data = []
    return data

def read_image_2d(image_file):
    image = PIL.Image.open(image_file)
    try:
        (width, height) = image.size
        data = list(image.getdata())
        data = numpy.array(data)
        data = data.reshape((height, width))
    except ValueError:
        data = []
    return data

########################################

def score_mpd(orig, relaxed): #Mean pixel difference
    orig_data = read_image(orig)
    relaxed_data = read_image(relaxed)

    error = 0
    total = 0
    for ppixel, apixel in itertools.izip(orig_data, relaxed_data):
        error += abs(ppixel - apixel)
        total += 1
    return error / MAX_PIXEL / total

def score_mae(orig, relaxed): #Mean absolute error 
    orig_data = read_image(orig)
    relaxed_data = read_image(relaxed)

    error = 0
    total = 0
    for ppixel, apixel in itertools.izip(orig_data, relaxed_data):
        error += abs(ppixel - apixel)
        total += 1
    return error / total

def score_rmse(orig, relaxed): #Root mean squared error
    orig_data = read_image(orig)
    relaxed_data = read_image(relaxed)

    error = 0
    total = 0
    for ppixel, apixel in itertools.izip(orig_data, relaxed_data):
        error += math.pow(abs(ppixel - apixel), 2)
        total += 1
    return math.sqrt((error / total))

def score_psnr(orig, relaxed): #Mean absolute error 
    mse = math.pow(score_rmse(orig, relaxed), 2)
    try:
        return 10*math.log(math.pow(MAX_PIXEL, 2)/mse)
    except:
        return float('Inf')

def score_me(orig, relaxed): #Misclassification error 
    orig_data = read_image(orig)
    relaxed_data = read_image(relaxed)

    incorrects = 0
    total = 0
    for ppixel, apixel in itertools.izip(orig_data, relaxed_data):
        if ppixel != apixel:
            incorrects += 1
        total += 1
    return incorrects / total

########################################

def fspecial_gauss(size, sigma):
    """Function to mimic the 'fspecial' gaussian MATLAB function
    """
    x, y = numpy.mgrid[-size//2 + 1:size//2 + 1, -size//2 + 1:size//2 + 1]
    g = numpy.exp(-((x**2 + y**2)/(2.0*sigma**2)))
    return g/g.sum()


def score_ssim(orig, relaxed, cs_map=False):
    """Return the Structural Similarity Map corresponding to input images img1 
    and img2 (images are assumed to be uint8)
    
    This function attempts to mimic precisely the functionality of ssim.m a 
    MATLAB provided by the author's of SSIM
    https://ece.uwaterloo.ca/~z70wang/research/ssim/ssim_index.m
    """

    img1 = numpy.asarray(PIL.Image.open(orig))
    img2 = numpy.asarray(PIL.Image.open(relaxed))

    img1 = img1.astype(numpy.float64)
    img2 = img2.astype(numpy.float64)
    size = 11
    sigma = 1.5
    window = fspecial_gauss(size, sigma)
    K1 = 0.01
    K2 = 0.03
    L = MAX_PIXEL #bitdepth of image
    C1 = (K1*L)**2
    C2 = (K2*L)**2
    mu1 = signal.fftconvolve(img1, window, mode='valid')
    mu2 = signal.fftconvolve(img2, window, mode='valid')
    mu1_sq = mu1*mu1
    mu2_sq = mu2*mu2
    mu1_mu2 = mu1*mu2
    sigma1_sq = signal.fftconvolve(img1*img1, window, mode='valid') - mu1_sq
    sigma2_sq = signal.fftconvolve(img2*img2, window, mode='valid') - mu2_sq
    sigma12 = signal.fftconvolve(img1*img2, window, mode='valid') - mu1_mu2
    if cs_map:
        return (((2*mu1_mu2 + C1)*(2*sigma12 + C2))/((mu1_sq + mu2_sq + C1)*
                    (sigma1_sq + sigma2_sq + C2)), 
                (2.0*sigma12 + C2)/(sigma1_sq + sigma2_sq + C2))
    else:
        return ((2*mu1_mu2 + C1)*(2*sigma12 + C2))/((mu1_sq + mu2_sq + C1)*
                    (sigma1_sq + sigma2_sq + C2))

def score_msssim(orig, relaxed):
    """This function implements Multi-Scale Structural Similarity (MSSSIM) Image 
    Quality Assessment according to Z. Wang's "Multi-scale structural similarity 
    for image quality assessment" Invited Paper, IEEE Asilomar Conference on 
    Signals, Systems and Computers, Nov. 2003 
    
    Author's MATLAB implementation:-
    http://www.cns.nyu.edu/~lcv/ssim/msssim.zip
    """

    img1 = numpy.asarray(PIL.Image.open(orig))
    img2 = numpy.asarray(PIL.Image.open(relaxed))

    level = 5
    weight = numpy.array([0.0448, 0.2856, 0.3001, 0.2363, 0.1333])
    downsample_filter = numpy.ones((2, 2))/4.0
    im1 = img1.astype(numpy.float64)
    im2 = img2.astype(numpy.float64)
    mssim = numpy.array([])
    mcs = numpy.array([])
    for l in range(level):
        ssim_map, cs_map = score_ssim(orig, relaxed, cs_map=True)
        mssim = numpy.append(mssim, ssim_map.mean())
        mcs = numpy.append(mcs, cs_map.mean())
        filtered_im1 = ndimage.filters.convolve(im1, downsample_filter, 
                                                mode='reflect')
        filtered_im2 = ndimage.filters.convolve(im2, downsample_filter, 
                                                mode='reflect')
        im1 = filtered_im1[::2, ::2]
        im2 = filtered_im2[::2, ::2]
    return (numpy.prod(mcs[0:level-1]**weight[0:level-1])*
                    (mssim[level-1]**weight[level-1]))

########################################
#from https://www.ugr.es/~jmaroza/inves/98ICS.pdf

WINDOW_HEIGHT = 3
WINDOW_WIDTH = 3

BIT = 0
HOLE = 255
EDGE = BIT
NOEDGE = HOLE

def edge_or_noedge(pixel_value):
    if pixel_value < 128:
        return EDGE
    else:
        return NOEDGE

def get_num_direct_mistakes(orig_data, relaxed_data, center, mistake_type):
    row = center[0]
    col = center[1]
    if orig_data[row][col] == edge_or_noedge(relaxed_data[row][col]):
        return 0
    elif edge_or_noedge(relaxed_data[row][col]) != mistake_type:
        return 0
    else:
        diff = 0
        if orig_data[row-1][col] != edge_or_noedge(relaxed_data[row-1][col]) and edge_or_noedge(relaxed_data[row-1][col]) == mistake_type:
            diff = diff + 1
        if orig_data[row+1][col] != edge_or_noedge(relaxed_data[row+1][col]) and edge_or_noedge(relaxed_data[row+1][col]) == mistake_type:
            diff = diff + 1
        if orig_data[row][col-1] != edge_or_noedge(relaxed_data[row][col-1]) and edge_or_noedge(relaxed_data[row][col-1]) == mistake_type:
            diff = diff + 1
        if orig_data[row][col+1] != edge_or_noedge(relaxed_data[row][col+1]) and edge_or_noedge(relaxed_data[row][col+1]) == mistake_type:
            diff = diff + 1
    return diff

def get_direct_NB(orig_data, relaxed_data, center):
    return get_num_direct_mistakes(orig_data, relaxed_data, center, BIT)

def get_direct_NH(orig_data, relaxed_data, center):
    return get_num_direct_mistakes(orig_data, relaxed_data, center, HOLE)

def get_num_mistakes(orig_data, relaxed_data, center, mistake_type):
    row = center[0]
    col = center[1]
    if orig_data[row][col] == edge_or_noedge(relaxed_data[row][col]):
        return 0
    elif edge_or_noedge(relaxed_data[row][col]) != mistake_type:
        return 0
    else:
        diff = 0
        for i in range(row-1, row+2):
            for j in range(col-1, col+2):
                if orig_data[i][j] != edge_or_noedge(relaxed_data[i][j]) and edge_or_noedge(relaxed_data[i][j]) == mistake_type:
                    diff = diff + 1
        diff = diff - 1
    return diff

def get_NB(orig_data, relaxed_data, center):
    return get_num_mistakes(orig_data, relaxed_data, center, BIT)

def get_NH(orig_data, relaxed_data, center):
    return get_num_mistakes(orig_data, relaxed_data, center, HOLE)

def get_NE(orig_data, center):
    row = center[0]
    col = center[1]
    edges = 0
    for i in range(row - int((WINDOW_HEIGHT-1)/2), row + int((WINDOW_HEIGHT-1)/2) + 1):
        for j in range(col - int((WINDOW_WIDTH-1)/2), col + int((WINDOW_WIDTH-1)/2) + 1):
            if orig_data[i][j] == EDGE:
                edges = edges + 1
    return edges

def is_mistake(orig_data, relaxed_data, center, mistake_type):
    row = center[0]
    col = center[1]
    if edge_or_noedge(relaxed_data[row][col]) != mistake_type:
        return False
    elif edge_or_noedge(relaxed_data[row][col]) == orig_data[row][col]:
        return False
    else:
        return True

a = 1.0
c = 1.0
p = 0.33
b = 0.5
h = 0.5
ibh =2.0
ihb = 2.0

def score_r(orig, relaxed):
    orig_data = read_image_2d(orig)
    relaxed_data = read_image_2d(relaxed)

    height = len(orig_data)
    width = len(orig_data[0])

    bits_sum = 0
    holes_sum = 0

    bits = 0
    holes = 0
    edges = 0
    for row in range(2, height-2):
        for col in range(2, width-2):
            edges += (orig_data[row][col] == EDGE)
            center = (row,col)
            #bits
            if is_mistake(orig_data, relaxed_data, (row,col), BIT):
                bits += 1
                bits_sum += (a*(1+b*get_NB(orig_data, relaxed_data, (row,col)))/(1+p*get_NE(orig_data, (row,col))+ibh*get_direct_NH(orig_data, relaxed_data, (row,col))))
            #holes
            if is_mistake(orig_data, relaxed_data, (row,col), HOLE):
                holes += 1
                holes_sum += (c*(1+h*get_NH(orig_data, relaxed_data, (row,col)))/(1+ihb*get_direct_NH(orig_data, relaxed_data, (row,col))))

    #print bits, holes, (bits+holes)/(height*width), (bits+holes)/edges
    #print bits_sum, holes_sum, (bits_sum+holes_sum)/(height*width), (bits_sum+holes_sum)/edges
    return (bits_sum+holes_sum)/(height*width), (bits_sum+holes_sum)/edges

########################################

if __name__ == '__main__':
    print 'me', score_me(sys.argv[1], sys.argv[2])
    print 'mpd', score_mpd(sys.argv[1], sys.argv[2])
    print 'mae', score_mae(sys.argv[1], sys.argv[2])
    print 'rmse', score_rmse(sys.argv[1], sys.argv[2])
    print 'psnr', score_psnr(sys.argv[1], sys.argv[2])
    print 'ssim', numpy.mean(score_ssim(sys.argv[1], sys.argv[2]))
    print 'msssim', score_msssim(sys.argv[1], sys.argv[2])
    print 'r', score_r(sys.argv[1], sys.argv[2])