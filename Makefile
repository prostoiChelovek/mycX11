build/%: %.c
	@mkdir -p build/
	gcc -ansi -Wall -Wpedantic -Werror -o $@ $<

