TEST_SRCS=$(wildcard tests/*.eve)
eve=../eve/build/eve
cache=tests/__eve__
build_path=../eve/build
out=tests/out.txt

clean:
	[ -f $(out) ] || exit 1;
	rm $(out)

test: $(TEST_SRCS)
	touch tests/out.txt
	for i in $^; do echo $$i; $(eve) $$i > $(out) 2>&1 || exit 1; echo OK; done
	tests/driver.sh
	rm -rf $(cache)

eve-debug:
	rm -rf $(build_path)
	mkdir $(build_path) && cd build && cmake -DCMAKE_BUILD_TYPE=Release -DEVE_DEBUG_MODE=ON .. && cmake --build .

eve:
	rm -rf $(build_path)
	mkdir $(build_path) && cd build && cmake -DCMAKE_BUILD_TYPE=Release -DEVE_DEBUG_MODE=OFF .. && cmake --build .

.PHONY: test clean