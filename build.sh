#!/bin/sh

set -xe

CFLAGS="-Wall -Wextra -framework IOKit -framework Cocoa -framework OpenGL \
-I/opt/homebrew/opt/raylib/include -Iraylib \
-I/opt/homebrew/opt/mysql/include/mysql -Imysql \
-I/opt/homebrew/opt/openssl@3/include -Iopenssl \
-Ibusiness -Ipersistence -Ipresentation -Iinfrastructure"

LDFLAGS="-L/opt/homebrew/opt/raylib/lib -lraylib \
-L/opt/homebrew/opt/mysql/lib -lmysqlclient \
-L/opt/homebrew/opt/zstd/lib -lzstd \
-L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto -lresolv -lm"

cc $CFLAGS $LDFLAGS -g -o bragibeats \
infrastructure/main.c \
business/audioProcessing.c \
presentation/uiRendering.c \
presentation/visualizers.c \
persistence/libraryInitialization.c

