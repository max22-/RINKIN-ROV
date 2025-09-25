FROM debian:trixie
WORKDIR /home/work

RUN apt update && apt install -y wget git

RUN wget https://go.dev/dl/go1.25.1.linux-amd64.tar.gz
RUN  rm -rf /usr/local/go && tar -C /usr/local -xzf go1.25.1.linux-amd64.tar.gz
ENV PATH="/usr/local/go/bin:${PATH}"
RUN git clone https://github.com/bluenviron/mediamtx
RUN cd mediamtx && git checkout v1.15.0 && go generate ./... && GOOS=linux GOARCH=arm64 CGO_ENABLED=0 go build .

ADD ./packages ./packages
RUN mkdir -p packages/mediamtx_1.15.0_arm64/usr/bin
RUN cp mediamtx/mediamtx packages/mediamtx_1.15.0_arm64/usr/bin
WORKDIR /home/work/packages
RUN dpkg-deb --build --root-owner-group mediamtx_1.15.0_arm64