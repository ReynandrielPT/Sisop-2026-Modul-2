CC     = gcc
CFLAGS = -Wall -Wextra

TARGETS = extract classify watcher report

all: $(TARGETS)
	@echo "[OK] Semua program berhasil dikompilasi."

extract: extract.c
	$(CC) $(CFLAGS) -o $@ $<

classify: classify.c
	$(CC) $(CFLAGS) -o $@ $<

watcher: watcher.c
	$(CC) $(CFLAGS) -o $@ $<

report: report.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean
