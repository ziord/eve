### eve

eve is a garbage-collected bytecode interpreter. See [tests](https://github.com/ziord/eve/tree/master/tests) directory for example programs.

### Building
```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_STANDARD=11 ..
cmake --build .

# testing
eve <filename.eve>
eve -h # for help
```

### Testing
`make test`

### License
[MIT](https://github.com/ziord/eve/blob/master/LICENSE.txt)