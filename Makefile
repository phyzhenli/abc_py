CC = g++
CFLAGS = -std=c++11 -DABC_USE_STDINT_H=1 -fPIC

ABC_DIR = ./abc
ABC_INC = -I$(ABC_DIR)/src
ABC_LIB = -L$(ABC_DIR) -labc
PYTHON_PLATFORM = x86_64-linux-gnu
PYTHON_VER = 3.8
PYTHON_INC = -I/usr/include/python$(PYTHON_VER)
PYTHON_LIB = -L/usr/lib/python$(PYTHON_VER)/config-$(PYTHON_VER)-$(PYTHON_PLATFORM) -lpython$(PYTHON_VER)
# BOOST_DIR  = ./boost
# BOOST_INC  = -I$(BOOST_DIR)
# BOOST_LIBS = -L$(BOOST_DIR)/stage/lib -lboost_python38 -lboost_numpy38 # -lboost_thread -lpthread 
BOOST_LIBS = -lboost_python38 -lboost_numpy38 # -lboost_thread -lpthread


TARGET = abc_py_.so

SRC = abc_py_.cpp

OBJS = $(SRC:.cpp=.o)

ALL: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -shared -Wl,--export-dynamic $^ $(ABC_LIB) $(PYTHON_LIB) $(BOOST_LIBS) -o $@


.PHONY:clean
clean:
	rm -rf $(TARGET) $(OBJS)


%.o:%.cpp
	$(CC) $(CFLAGS) $(ABC_INC) $(BOOST_INC) $(PYTHON_INC) -o $@ -c $<
