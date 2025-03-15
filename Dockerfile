FROM ubuntu:14.04

LABEL name="dsb4"
LABEL lab.number="4"

RUN apt-get update
RUN apt-get -y --allow-unauthenticated install clang-3.5 \
make

RUN mkdir /app
WORKDIR /app
COPY ./ /app