NAME = webserv
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g3 -DLOG_LEVEL=0
SRCDIR = src
INCDIR = include
OBJDIR = obj

SOURCES = main.cpp WebServer.cpp HttpRequest.cpp Config.cpp ConfigUtils.cpp utils.cpp
OBJECTS = $(SOURCES:%.cpp=$(OBJDIR)/%.o)
SRCFILES = $(addprefix $(SRCDIR)/, $(SOURCES))

all: $(NAME)

$(NAME): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(NAME)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re