#!/bin/bash
tmp=$(mktemp -d /tmp/eve-test-XXXXXX)
eve=../eve/build/eve
trap 'rm -rf $tmp' INT TERM HUP EXIT
echo > "$tmp"/empty.eve

check() {
  if [ $? -eq 0 ]; then
    echo "testing $1 ...passed"
  else
    echo "testing $1 ...failed"
    exit 1
  fi
}

# --help
${eve} -h | grep -q Eve
check -h

# general options
${eve} | grep -q Usage
check options

echo OK
