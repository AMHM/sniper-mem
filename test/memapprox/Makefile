TARGET=memapprox
include ../shared/Makefile.shared

$(TARGET): $(TARGET).o
	$(CC) $(TARGET).o -lm $(SNIPER_LDFLAGS) -o $(TARGET)

run_$(TARGET):
	$(MAKE) -C /workspace/sniper/sniper-6.1/
	../../run-sniper -s	biswadip.py -n 2 -c gainestown -g log/enabled="true" -g fault_injection/injector="range" -g fault_injection/type="toggle" --roi --viz -- ./memapprox -p 2

CLEAN_EXTRA=viz
