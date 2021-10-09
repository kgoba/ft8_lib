
RM = rm
MKDIR = mkdir

OBJDIR = obj
SODIR = bin
SRCDIR = src
LIBS = libft8.so
LIBSA = libft8.a
LIBFILE = libft8.so.0.0
OUTFILE = $(SODIR)/$(LIBFILE)

LDFLAGS = -shared -Wl,-soname,$(LIBFILE)

INC = -I./include

CFLAGS += -fPIC -w

SRCS := $(wildcard *.c $(SRCDIR)/*.c)
OBJS := $(patsubst %.c, $(OBJDIR)/%.o, $(notdir ${SRCS}))


$(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) -c -o "$(OBJDIR)/$(*F).o" $(INC) $(CFLAGS) "$<" 

all: $(OUTFILE)

$(OUTFILE): $(OBJDIR) $(OBJS)
	$(CC) -o "$(OUTFILE)" $(OBJS) $(LDFLAGS)
	ln -sf $(LIBFILE) $(SODIR)/$(LIBS)


$(OBJDIR):
	$(MKDIR) -p "$(OBJDIR)"
	$(MKDIR) -p "$(SODIR)"


rebuild: cleanall all

clean:
	$(RM) -f $(OUTFILE)
	$(RM) -f $(OBJS)
	$(RM) -rf $(OBJDIR)
	$(RM) -rf $(SODIR)


cleanall: clean


install:
	$(INSTALL) -D $(OUTFILE) /usr/lib/$(LIBFILE)


