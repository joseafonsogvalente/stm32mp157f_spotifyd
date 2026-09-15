How to test the UI

1) Install fedora packages

sudo dnf install gtk3-devel gdk-pixbuf2-devel cairo-devel gcc pkg-config

2) Compile
gcc ~/repos/stm32mp157f_spotifyd/UI/main.c -o spotify-ui-preview $(pkg-config --cflags --libs gtk+-3.0 gdk-pixbuf-2.0 cairo)

3) Run
./spotify-ui-preview
