TEST_SRCS=$(wildcard tests/*.eve)
eve=../eve/build/eve
cache=tests/__eve__
out=tests/out.txt

clean:
	[ -f $(out) ] || exit 1;
	rm $(out)

test: $(TEST_SRCS)
	touch tests/out.txt
	for i in $^; do echo $$i; $(eve) $$i > $(out) 2>&1 || exit 1; echo OK; done
	tests/driver.sh
	rm -rf $(cache)

.PHONY: test clean