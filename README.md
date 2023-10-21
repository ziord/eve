### eve

eve is a garbage-collected bytecode interpreter. See the [tests](https://github.com/ziord/eve/tree/master/tests) directory for example programs.

### Building
```
# debug mode
make eve-debug

# release mode
make eve

# testing the interpreter
./build/eve <filename.eve>
./build/eve -h # for help
```

### Testing
Run test suites:
`make test`

### License
[MIT](https://github.com/ziord/eve/blob/master/LICENSE.txt)