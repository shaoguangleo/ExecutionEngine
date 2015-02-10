all: EEngine

server: server.cpp
	mpic++ -o server server.cpp

client: client.cpp
	mpic++ -o client client.cpp

EEngine: EEngine.cpp
	mpic++ -o EEngine EEngine.cpp -pthread
