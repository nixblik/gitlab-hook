#!/bin/sh

[ -f key.pem  ] || certtool --generate-privkey --outfile key.pem --ecc

[ -f cert.pem ] || certtool --generate-self-signed --load-privkey key.pem \
                            --template cert.cfg --outfile cert.pem
