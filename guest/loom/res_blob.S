/* res.bin de LOOM-8 (909 cymbales, grands caractères : tools/gen_res.py), lu en place par plat_res */
    .section .rodata.loom_res, "a"
    .balign 4
    .global loom_res
    .global loom_res_end
loom_res:
    .incbin LOOM_RES_BIN
loom_res_end:
