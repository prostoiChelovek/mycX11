build/%: %.c
	@mkdir -p build/
	gcc -ansi -Wall -Wpedantic -Werror -g -o $@ $<

