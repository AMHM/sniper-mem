TARGET=memapprox
include ../shared/Makefile.shared

$(TARGET): $(TARGET).o
	$(CXX) $(TARGET).o -lm $(SNIPER_LDFLAGS) -o $(TARGET)

run_$(TARGET):
	$(MAKE) -C ../../
	../../run-sniper \
	-n 1 \
	-c gainestown \
	-g fault_injection/injector="range" \
	-g fault_injection/type="toggle" \
	-g fault_injection/affected="dram" \
	-g log/enabled="true" \
	--viz \
	-- ./memapprox

CLEAN_EXTRA=viz
