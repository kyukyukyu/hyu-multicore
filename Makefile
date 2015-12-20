.SUFFIXES : .cc .o
#컴파일러 지정
CXX = g++

#CXXFLAGS : -g : 프로그램 에러 시 디버깅용 파일 생성(core)
#           -Wall : 컴파일 에러 시 메시지 출력
#           -std=c++11 : 표준 C++11 문법 사용
CXXFLAGS = -g -Wall -std=c++11
all: hw

#아래의 매크로들은 컴파일 시 경로지정을 위해 필요
OBJS = ./src/main.o ./src/trx.o
SRCS = ./src/$(OBJS:.o=.cc)
BIN = ./bin/
LIB = ./lib
INCLUDE = ./include
LIBNAME = -lpthread

#CXXFLAGS에 -I옵션 추가를 통해 헤더파일 위치 지정
CXXFLAGS += -I$(INCLUDE)

#컴파일 옵션을 지정. 라이브러리 관련 gcc옵션은 이곳에 추가하여야 함
hw: $(OBJS)
	$(CXX) -o $(BIN)homework $(OBJS) $(LIBNAME) -L$(LIB)

#make clean시 수행할 명령어 (여기서는 오브젝트 파일 지우기)
clean:
	rm -rf $(OBJS)
#make allclean시 수행할 명령어 (여기서는 오브젝트 파일 및 bin폴더 내 파일 지우기)
allclean:
	rm -rf $(OBJS) $(BIN)*
