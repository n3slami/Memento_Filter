FROM ubuntu:latest

WORKDIR /usr/local/

# Install dependencies
RUN apt-get update && apt-get upgrade -y
#RUN apt-get install texlive-full -y
RUN apt-get install zstd -y
RUN apt-get install wget -y
RUN apt-get install coreutils -y
RUN apt-get install curl -y
RUN apt-get install build-essential -y
RUN apt-get install git -y
RUN apt-get install gcc -y
RUN apt-get install g++ -y
RUN apt-get install make -y
RUN apt-get install cmake -y
RUN apt-get install libboost-all-dev -y
RUN apt-get install python3-venv -y
RUN apt-get install python3-dev -y
RUN apt-get install python3-pip -y
RUN apt-get install libssl-dev -y
RUN apt-get install bc -y

RUN git clone https://github.com/n3slami/Memento_Filter.git

WORKDIR /usr/local/Memento_Filter
SHELL [ "/bin/bash", "-c" ]
ENTRYPOINT [ "./evaluate.sh" ]

