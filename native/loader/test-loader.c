/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main loader_pid1_main
#include "loader.c"
#undef main
#include <assert.h>

static void reset_status(void) {
    memset(&status,0,sizeof(status));status.stage="test";status.module_stage="not_attempted";
}
static void digest_hex(const char *input, const char *expected) {
    unsigned char digest[32];char hex[65];
    sha256((const unsigned char *)input,strlen(input),digest);
    for(unsigned i=0;i<32;i++)snprintf(hex+i*2,3,"%02x",digest[i]);
    assert(!strcmp(hex,expected));
}
int main(int argc,char **argv) {
    assert(argc==2);
    digest_hex("","e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    digest_hex("abc","ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    FILE *f=fopen(argv[1],"rb");assert(f);
    unsigned char *original=malloc(EXPECTED_PROBE_BYTES);assert(original);
    assert(fread(original,1,EXPECTED_PROBE_BYTES,f)==EXPECTED_PROBE_BYTES);fclose(f);
    unsigned char *copy=malloc(EXPECTED_PROBE_BYTES);assert(copy);
    memcpy(copy,original,EXPECTED_PROBE_BYTES);
    struct binding bindings[PIN_COUNT]={0};
    reset_status();assert(validate_elf(copy,EXPECTED_PROBE_BYTES,bindings)==0);
    assert(status.symbols==PIN_COUNT);
    /* A one-byte input change or different byte count cannot reach binding/load. */
    copy[0]^=1;reset_status();assert(validate_elf(copy,EXPECTED_PROBE_BYTES,bindings)==EKEYREJECTED);
    memcpy(copy,original,EXPECTED_PROBE_BYTES);reset_status();
    assert(validate_elf(copy,EXPECTED_PROBE_BYTES-1,bindings)==EKEYREJECTED);
    memset(bindings,0,sizeof(bindings));reset_status();
    assert(validate_elf(copy,EXPECTED_PROBE_BYTES,bindings)==0);
    char symbol_text[16384];size_t used=0;
    /* Zero addresses and module-owned names must not satisfy core resolution. */
    for(unsigned i=0;i<PIN_COUNT;i++)used+=(size_t)snprintf(symbol_text+used,sizeof(symbol_text)-used,"0000000000000000 T %s\n",allowed[i]);
    for(unsigned i=0;i<PIN_COUNT;i++)used+=(size_t)snprintf(symbol_text+used,sizeof(symbol_text)-used,"ffffffff81234567 T %s [other]\n",allowed[i]);
    f=fmemopen(symbol_text,used,"r");assert(f);
    assert(resolve_symbols(f,bindings)==ENOENT);fclose(f);
    assert(status.resolved==0 && status.missing==PIN_COUNT && !memcmp(copy,original,EXPECTED_PROBE_BYTES));
    used=0;
    for(unsigned i=0;i<PIN_COUNT-1;i++)used+=(size_t)snprintf(symbol_text+used,sizeof(symbol_text)-used,"%016llx T %s\n",0xffffffff81000000ULL+i*16,allowed[i]);
    f=fmemopen(symbol_text,used,"r");assert(f);
    assert(resolve_symbols(f,bindings)==ENOENT);fclose(f);
    assert(status.missing==1 && !memcmp(copy,original,EXPECTED_PROBE_BYTES));
    /* Re-validate/reset before success, then bind all 24. */
    memset(bindings,0,sizeof(bindings));reset_status();
    assert(validate_elf(copy,EXPECTED_PROBE_BYTES,bindings)==0);
    used=0;
    for(unsigned i=0;i<PIN_COUNT;i++)used+=(size_t)snprintf(symbol_text+used,sizeof(symbol_text)-used,"%016llx T %s\n",0xffffffff81000000ULL+i*16,allowed[i]);
    f=fmemopen(symbol_text,used,"r");assert(f);
    assert(resolve_symbols(f,bindings)==0);fclose(f);
    assert(status.resolved==PIN_COUNT && status.missing==0);
    for(unsigned i=0;i<PIN_COUNT;i++) {
        assert(bindings[i].sym->st_shndx==SHN_ABS);
        assert(bindings[i].sym->st_value==0xffffffff81000000ULL+i*16);
    }
    /* Exactly st_shndx/st_value of the whitelisted symbols may change. */
    for(size_t i=0;i<EXPECTED_PROBE_BYTES;i++) {
        if(copy[i]==original[i])continue;
        int permitted=0;
        for(unsigned j=0;j<PIN_COUNT;j++) {
            size_t offset=(unsigned char *)bindings[j].sym-copy;
            permitted|=(i>=offset+offsetof(Elf64_Sym,st_shndx)&&i<offset+offsetof(Elf64_Sym,st_shndx)+sizeof(bindings[j].sym->st_shndx));
            permitted|=(i>=offset+offsetof(Elf64_Sym,st_value)&&i<offset+offsetof(Elf64_Sym,st_value)+sizeof(bindings[j].sym->st_value));
        }
        assert(permitted);
    }
    free(copy);free(original);
    puts("Pinned ELF, SHA256 vectors, changed input, missing/zero/module-only symbol rejection and exact binding mutation passed");
    return 0;
}
