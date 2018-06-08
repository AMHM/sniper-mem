double score_me(unsigned char *original, unsigned char *relaxed, int width, int height) 
{
    int incorrects = 0;
    int total = 0;

    for(int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            if (original[i*width + j] != relaxed[i*width + j]) {
                incorrects += 1;
            }
            total += 1;
        }
    }
    
    return (double)incorrects / (double)total;
}

//========================================================================================

#define WINDOW_HEIGHT 3
#define WINDOW_WIDTH 3

#define BIT 0
#define HOLE 255
#define EDGE BIT
#define NOEDGE HOLE

unsigned char edge_or_noedge(unsigned char pixel_value) 
{
    if (pixel_value < 128)
        return EDGE;
    else
        return NOEDGE;
}

unsigned int get_num_direct_mistakes(unsigned char *orig_data, unsigned char *relaxed_data, 
                                     int width, int height, 
                                     int center_x, int center_y, unsigned char mistake_type)
{

    int row = center_y;
    int col = center_x;
    if (orig_data[row * width + col] == edge_or_noedge(relaxed_data[row * width + col]))
        return 0;
    else if (edge_or_noedge(relaxed_data[row * width + col]) != mistake_type)
        return 0;
    else {
        unsigned int diff = 0;
        if (orig_data[(row-1) * width + col] != edge_or_noedge(relaxed_data[(row-1) * width + col]) && edge_or_noedge(relaxed_data[(row-1) * width + col]) == mistake_type)
            diff = diff + 1;
        if (orig_data[(row+1) * width + col] != edge_or_noedge(relaxed_data[(row+1) * width + col]) && edge_or_noedge(relaxed_data[(row+1) * width + col]) == mistake_type)
            diff = diff + 1;
        if (orig_data[row * width + col-1] != edge_or_noedge(relaxed_data[row * width + col-1]) && edge_or_noedge(relaxed_data[row * width + col-1]) == mistake_type)
            diff = diff + 1;
        if (orig_data[row * width + col+1] != edge_or_noedge(relaxed_data[row * width + col+1]) && edge_or_noedge(relaxed_data[row * width + col+1]) == mistake_type)
            diff = diff + 1;

        return diff;
    }

}

unsigned int get_direct_NB(unsigned char *orig_data, unsigned char *relaxed_data, 
                           int width, int height, 
                           int center_x, int center_y)
{
    return get_num_direct_mistakes(orig_data, relaxed_data, width, height, center_x, center_y, BIT);
}

unsigned int get_direct_NH(unsigned char *orig_data, unsigned char *relaxed_data, 
                           int width, int height, 
                           int center_x, int center_y)
{
    return get_num_direct_mistakes(orig_data, relaxed_data, width, height, center_x, center_y, HOLE);
}

unsigned int get_num_mistakes(unsigned char *orig_data, unsigned char *relaxed_data, 
                              int width, int height, 
                              int center_x, int center_y, unsigned char mistake_type)
{

    int row = center_y;
    int col = center_x;
    if (orig_data[row * width + col] == edge_or_noedge(relaxed_data[row * width + col]))
        return 0;
    else if (edge_or_noedge(relaxed_data[row * width + col]) != mistake_type)
        return 0;
    else {
        unsigned int diff = 0;
        for (int i = row-1; i < row+2; i++) {
            for (int j = col-1; j < col+2; j++) {
                if (orig_data[i * width + j] != edge_or_noedge(relaxed_data[i * width + j]) && edge_or_noedge(relaxed_data[i * width + j]) == mistake_type)
                    diff = diff + 1;
            }
        }
        diff = diff - 1; // not including self
        return diff;
    }

}

unsigned int get_NB(unsigned char *orig_data, unsigned char *relaxed_data, 
                    int width, int height, 
                    int center_x, int center_y)
{
    return get_num_mistakes(orig_data, relaxed_data, width, height, center_x, center_y, BIT);
}

unsigned int get_NH(unsigned char *orig_data, unsigned char *relaxed_data, 
                    int width, int height, 
                    int center_x, int center_y)
{
    return get_num_mistakes(orig_data, relaxed_data, width, height, center_x, center_y, HOLE);
}

unsigned int get_NE(unsigned char *orig_data, 
                    int width, int height, 
                    int center_x, int center_y)
{
    int row = center_y;
    int col = center_x;
    unsigned int edges = 0;
    for (int i = row - (WINDOW_HEIGHT-1)/2; i < row + (WINDOW_HEIGHT-1)/2 + 1; i++){
        for (int j = col - (WINDOW_WIDTH-1)/2; j < col + (WINDOW_WIDTH-1)/2 + 1; j++){
            if (orig_data[i * width + j] == EDGE)
                edges = edges + 1;
        }
    }
    return edges;
}

unsigned int is_mistake(unsigned char *orig_data, unsigned char *relaxed_data, 
                        int width, int height, 
                        int center_x, int center_y, unsigned char mistake_type)
{
    int row = center_y;
    int col = center_x;
    if (edge_or_noedge(relaxed_data[row * width + col]) != mistake_type)
        return false;
    else if (edge_or_noedge(relaxed_data[row * width + col]) == orig_data[row * width + col])
        return false;
    else
        return true;
}

double a = 1.0;
double c = 1.0;
double p = 0.33;
double b = 0.5;
double h = 0.5;
double ibh = 2.0;
double ihb = 2.0;

double score_r(unsigned char *orig_data, unsigned char *relaxed_data, int width, int height)
{
    unsigned int bits_sum = 0;
    unsigned int holes_sum = 0;

    unsigned int bits = 0;
    unsigned int holes = 0;
    unsigned int edges = 0;
    for (int row = 2; row < height-2; row++){
        for (int col = 2; col < width-2; col++) {
            edges += (orig_data[row * width + col] == EDGE);
            //bits
            if (is_mistake(orig_data, relaxed_data, width, height, col, row, BIT)) {
                bits += 1;
                bits_sum += (a*(1+b*get_NB(orig_data, relaxed_data, width, height, col, row))/(1+p*get_NE(orig_data, width, height, col, row)+ibh*get_direct_NH(orig_data, relaxed_data, width, height, col, row)));
            }
            //holes
            if (is_mistake(orig_data, relaxed_data, width, height, col, row, HOLE)) {
                holes += 1;
                holes_sum += (c*(1+h*get_NH(orig_data, relaxed_data, width, height, col, row))/(1+ihb*get_direct_NH(orig_data, relaxed_data, width, height, col, row)));
            }
        }
    }

    //print bits, holes, (bits+holes)/(height*width), (bits+holes)/edges
    //print bits_sum, holes_sum, (bits_sum+holes_sum)/(height*width), (bits_sum+holes_sum)/edges
    return (double)(bits_sum+holes_sum)/(double)(height*width); //(bits_sum+holes_sum)/edges
}
