.SUFFIXES : .c .o
#컴파일러 지정
CC = gcc

#CFLAGS : -g : 프로그램 에러 시 디버깅용 파일 생성(core)
#         -Wall : 컴파일 에러 시 메시지 출력
#         -ansi : 표준 ANSI C 문법 사용
CFLAGS = -g -Wall -ansi
all: hw

#아래의 매크로들은 컴파일 시 경로지정을 위해 필요
OBJS = ./src/main.o ./src/prime_numbers.o ./src/taskqueue.o
SRCS = ./src/$(OBJS:.o=.c)
BIN = ./bin/
LIB = ./lib
INCLUDE = ./include
LIBNAME = -lpthread -lm

#CFLAGS에 -I옵션 추가를 통해 헤더파일 위치 지정
CFLAGS += -I$(INCLUDE)

#컴파일 옵션을 지정. 라이브러리 관련 gcc옵션은 이곳에 추가하여야 함
hw: $(OBJS)
	$(CC) -o $(BIN)homework $(OBJS) $(LIBNAME) -L$(LIB)

#make clean시 수행할 명령어 (여기서는 오브젝트 파일 지우기)
clean:
	rm -rf $(OBJS)
#make allclean시 수행할 명령어 (여기서는 오브젝트 파일 및 bin폴더 내 파일 지우기)
allclean:
	rm -rf $(OBJS) $(BIN)*
