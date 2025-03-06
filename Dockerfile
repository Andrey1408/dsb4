FROM ubuntu:14.04

LABEL name="dsb3"
LABEL lab.number="3"

RUN apt-get update
RUN apt-get -y --allow-unauthenticated install clang-3.5 \
make
RUN apt-get -y install valgrind

RUN mkdir /app
WORKDIR /app
COPY ./ /app
RUN make all