FROM alpine:3.14

WORKDIR /usr/local/

# Install dependencies
RUN apk update && apk upgrade
RUN apk add texlive-full
RUN apk add zstd
RUN apk add wget
RUN apk add coreutils
RUN apk add curl
RUN apk add git
RUN apk add g++
RUN apk add make
RUN apk add cmake
RUN apk add boost-dev
RUN apk add py-pip
RUN apk add openssl-dev

RUN git clone https://github.com/n3slami/Memento_Filter.git

WORKDIR /usr/local/Memento_Filter
SHELL [ "/bin/bash", "-c" ]
ENTRYPOINT [ "./evaluate.sh" ]

