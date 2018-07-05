TARGET=memapprox
include ../shared/Makefile.shared

$(TARGET): $(TARGET).o
	$(CC) $(TARGET).o -lm $(SNIPER_LDFLAGS) -o $(TARGET)

run_$(TARGET):
	$(MAKE) -C ../../
	../../run-sniper -n 2 -c gainestown -g fault_injection/injector="range" -g fault_injection/type="toggle" -g fault_injection/affected="L2" --roi --viz -- ./memapprox

CLEAN_EXTRA=viz
