#!/bin/bash
eve=../eve/build/eve

check() {
  if [ $? -eq 0 ]; then
    echo "testing $1 ...passed"
  else
    echo "testing $1 ...failed"
    exit 1
  fi
}

# -h
${eve} -h | grep -q "Eve interpreter"
check -h

# --help
${eve} --help | grep -q "Eve interpreter"
check --help

# -v
${eve} -v | grep -q Eve
check -v

# --version
${eve} --version | grep -q Eve
check --version

# general options
${eve} | grep -q Usage
check options

echo OK
