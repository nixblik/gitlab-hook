#!/bin/sh

DIR=`dirname $0`
curl -d@${DIR}/pipeline_event.json https://localhost:8080/debug \
     --header "X-Gitlab-Token: abcd" --header "X-Gitlab-Event: Pipeline Hook" \
     --cacert ${DIR}/cert/cert.pem
