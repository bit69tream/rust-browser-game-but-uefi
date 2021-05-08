EFIINC ?= "/usr/include/efi"
EFILIB ?= "/usr/lib64/"

build: frontend.o backend.o
	${LD} $^                           \
	      ${EFILIB}/crt0-efi-x86_64.o  \
	      -nostdlib                    \
	      -znocombreloc                \
	      --warn-common                \
	      --no-undefined               \
	      --fatal-warnings             \
	      --build-id=sha1              \
	      -shared                      \
	      -Bsymbolic                   \
	      -L${EFILIB}                  \
	      -lefi -lgnuefi               \
	      -o game.so                   \
	      -T ${EFILIB}/elf_x86_64_efi.lds

	objcopy -j .text                \
	        -j .sdata               \
	        -j .data                \
	        -j .dynamic             \
	        -j .dynsym              \
	        -j .rel                 \
	        -j .rela                \
	        -j .rel.*               \
	        -j .rela.*              \
	        -j .rel*                \
	        -j .rela*               \
	        -j .reloc               \
	        --target efi-app-x86_64 \
	        game.so                 \
	        game.efi

.PHONY: frontend.o
frontend.o: frontend.c
	${CC}                           \
		-I${EFIINC}                 \
		-I${EFIINC}/x86_64          \
		-pedantic					\
		-Wall						\
		-Wextra						\
		-Wcast-align				\
		-Wcast-qual					\
		-Wdisabled-optimization		\
		-Wformat=2					\
		-Winit-self					\
		-Wlogical-op				\
		-Wmissing-declarations		\
		-Wmissing-include-dirs		\
		-Wredundant-decls			\
		-Wshadow					\
		-Wsign-conversion			\
		-Wstrict-overflow=5			\
		-Wswitch-default			\
		-Wundef						\
		-Werror						\
		-Wunused					\
		-mno-red-zone				\
		-fpic						\
		-std=c11					\
		-Os							\
		-fno-builtin				\
		-fshort-wchar				\
		-ffreestanding				\
		-nostdlib                   \
		-Wno-missing-declarations   \
		-Wno-undef                  \
		-Wno-redundant-decls        \
		-fno-strict-aliasing        \
		-fno-stack-protector        \
		-fno-stack-check            \
		-fno-merge-all-constants    \
		-DCONFIG_x86_64             \
		-DEFI_FUNCTION_WRAPPER      \
		-DGNU_EFI_USE_MS_ABI        \
		-maccumulate-outgoing-args  \
		-D__KERNEL__                \
		-c $^						\
		-o $@

.PHONY: backend.o
backend.o: backend.rs
	rustc -C opt-level=s -C panic=abort $^ --emit=obj -o $@

.PHONY: image
image:
	dd if=/dev/zero of=game.img bs=1M count=2
	mformat -f 1440 -i game.img
	mmd -i game.img ::/EFI
	mmd -i game.img ::/EFI/BOOT
	mcopy -i game.img game.efi ::/EFI/BOOT/BOOTX64.efi
