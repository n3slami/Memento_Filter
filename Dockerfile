FROM alpine:3.14

WORKDIR /usr/local/

RUN apk update && apk upgrade
RUN apk add zstd wget coreutils curl git g++ make cmake boost-dev texlive-full py-pip
RUN apk add openssl-dev

RUN git clone https://github.com/n3slami/Memento_Filter.git

WORKDIR /usr/local/Memento_Filter
RUN chmod +x evaluate.sh
SHELL [ "/bin/bash", "-c" ]
ENTRYPOINT [ "./evaluate.sh" ]

