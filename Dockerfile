FROM gcc

WORKDIR /usr/src/app
COPY . .

RUN make

WORKDIR /usr/src/app/server

CMD ["./upserver"]

