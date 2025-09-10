main.wasm: main.c
	clang-mp-19 --target=wasm32 -O3 -nostdlib -Wl,--no-entry -Wl,--export=init -Wl,--export=frame -Wl,--export=set_viewport -Wl,--export-memory -Wl,--initial-memory=1048576 -Wl,--max-memory=16777216 -Wl,--export-table -Wl,--allow-undefined -o $@ $<
