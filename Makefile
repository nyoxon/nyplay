CC := gcc
CFLAGS := -Wall -Wextra -g
LDLIBS = -lasound

TARGET = nyplay

SRCDIR = src
OBJDIR = build

SRCS = player.c cli_interface.c fd_handle.c sound_engine.c types.c
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)
	
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)