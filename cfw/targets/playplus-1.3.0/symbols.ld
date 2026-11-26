/* Play+ 1.3.0 firmware symbols used by CFW patches (our names). Functions carry the Thumb bit.
 * Evidence for each entry: docs/FIRMWAfirmware notes.md. */

/* functions */
fw_operator_new        = 0x600aab65; /* thunk_FUN_600ad664(size) -> void* */
fw_menu_submenu_init   = 0x600e9d75; /* FUN_600e9d74(item, parent, label, builder_ptr, builder_adj) */
fw_menu_action_init    = 0x600e7861; /* FUN_600e7860(item, menu, label, std::function*, flag) */
fw_menu_build_root     = 0x600e9e59; /* stock root "Menu" builder (ctx, menu) */
fw_menu_option_init    = 0x600e84a1; /* FUN_600e84a0(item 0x60, menu, label, u32 *val, max index, labels[], std::function *on_change): stock option row ("Grid Brightness  High") */
fw_menu_build_samples  = 0x600e7995; /* stock "Samples" builder FUN_600e7994(ctx, menu) */
fw_menu_close          = 0x600e6a1d; /* FUN_600e6a1c(ctx): close menu, back to previous screen */
fw_fn_manager_word     = 0x600e44cd; /* std::function manager for a 1-word capture */
fw_undo_push           = 0x600ecfb9; /* FUN_600ecfb8(ctrl+0x23a8, label) */
fw_toast_show          = 0x600d99c1; /* FUN_600d99c0(ui+0x148, msg) */
fw_audio_irq_off       = 0x600b1121; /* FUN_600b1120: NVIC ICER0 = 1 */
fw_audio_irq_on        = 0x600b1115; /* FUN_600b1114: NVIC ISER0 = 1 */

fw_menu_screen_update  = 0x601057c1; /* menu screen vt+0x0c FUN_601057c0(scr) */
fw_menu_screen_hide    = 0x60104e31; /* menu screen vt+0x08 FUN_60104e30(scr): destroys its widgets */
fw_eve_begin           = 0x600a95f9; /* FUN_600a95f8: CS low, write address RAM_CMD + wp */
fw_eve_end             = 0x600a9619; /* FUN_600a9618: CS high, REG_CMD_WRITE = wp */
fw_eve_word            = 0x600a9c91; /* FUN_600a9c90(word): 4 bytes into the open burst */
fw_eve_idle            = 0x600a9639; /* FUN_600a9638: !=0 when CMD_READ == CMD_WRITE (resyncs wp) */
fw_eve_text            = 0x600a9a1d; /* FUN_600a9a1c(x, y, font, opts, str, maxlen): CMD_TEXT */
fw_eve_dlstart         = 0x600a9bc5; /* FUN_600a9bc4: CMD_DLSTART */
fw_w9_append           = 0x600b1655; /* rect widget (type 9) vtable 0x600770fc: [1] append */
fw_w9_cache            = 0x600b1629; /* [2] cache: MEMCPY RAM_DL -> RAM_G slot, size = REG_CMD_DL */
fw_w9_nop              = 0x600b15d5; /* [3],[4],[5],[7],[8]: bx lr */
fw_w9_set_colors       = 0x600b15dd; /* [6]: str r1, [r0, #0x30] */
fw_w9_delete           = 0x600b15e1; /* [9]: operator delete(this, 0x38) */

fw_menu_build_synth    = 0x600ea4c9; /* FUN_600ea4c8(ctx, menu, slot): per-synth submenu builder */

/* data */
fw_topt_none           = 0x60078204; /* const u32 0x7fedcba9: tiny_optional<float> without value */
fw_eve_wp              = 0x20002816; /* u16 RAM_CMD write offset (DAT_20002816) */
fw_audio_render_cb     = 0x2000301c; /* (AvrekStudio) pointeur du callback de rendu audio appelé par l'IRQ audio 0x13a18 ; 0x600ad379 après démarrage (vu sur le Play+ virtuel) */
fw_audio_out           = 0x2000281c; /* s16[2][128][2] audio DMA buffer, final output (audio IRQ 0x13a18) */
fw_edit_lock_depth     = 0x2001daf4; /* nesting counter for the audio-IRQ edit lock */
cfw_ram                = 0x20002700; /* 256 B scratch in the unused DTCM gap 0x200022E8..0x200027FF */

/* screen brightness / dim (screen.c) */
fw_app_update          = 0x600c84f5; /* app vtable 0x60078224 slot 1: called once per main-loop pass */
fw_global_settings_save = 0x600c848d; /* (AvrekStudio) écrit Projects/globalSettings sur la carte (r0 = app). Appelée par fw_app_update (bl en 0x600c8576) quand l'empreinte des 15 mots de réglages globaux (app + 0x4664 .. ; Master Volume = app + 0x4674) change ; empreinte rangée à app + 0x47d0. Vu au virtuel 26/11/2026 : 1 écriture par cran de Master */
fw_menu_build_settings = 0x600eac41; /* FUN_600eac40(ctx, menu): stock Settings builder */
fw_menu_back           = 0x60105085; /* FUN_60105084(menu screen): one menu level back */
fw_file_open           = 0x600aa8d1; /* FUN_600aa8d0(file, path, mode) -> 1 ok (File wrapper, 0x70 B) */
fw_file_read           = 0x600aa921; /* FUN_600aa920(file, buf, n) -> bytes or -1 */
fw_file_write          = 0x600aa95d; /* FUN_600aa95c(file, buf, n) -> bytes or -1 */
fw_file_close          = 0x600aa895; /* FUN_600aa894(file) */
fw_display_update      = 0x600ab395; /* (AvrekStudio) 0x600ab394(dm) : automate d'affichage (dm+0x14 état, dm+0x34 prêt), appelé en 0x600aab34 */
fw_pad_leds            = 0x20004d84; /* (AvrekStudio) pilote LED des pads : +0 état, +4 trame 0x1E30 o, +8 modifiée ; = gestionnaire 0x20004d80 + 4 */
fw_led_encode          = 0x600af999; /* (AvrekStudio) 0x600af998(pilote, i, r, g, b sur la pile) : encode la LED i (i = rangée x 20 + colonne) */
fw_pad_led_set         = 0x600ac581; /* (AvrekStudio) 0x600ac580(gest, i, couleur*, couleur2*, mode) : vtable 0x60076eb8 slot 1 */
fw_eve_cs_low          = 0x600a9535; /* (AvrekStudio) EVE : CS bas + attente 1 us (début d'une transaction SPI, appelé par 0x600a95f8) */
fw_eve_cs_high         = 0x600a9559; /* (AvrekStudio) EVE : attente 1 us + CS haut (fin de transaction) */
fw_eve_pause           = 0x600a9595; /* (AvrekStudio) EVE : attente 3 us (après CS bas et après CS haut, comme 0x600a95f8 / 0x600a9618) */
fw_eve_wr_addr         = 0x600a9c6d; /* (AvrekStudio) EVE : envoie l'adresse 22 bits d'une écriture (octet haut | 0x80) */
fw_eve_spi_byte        = 0x600a957d; /* (AvrekStudio) EVE : un octet sur le SPI (0x600afe5c) */
fw_lpspi_xfer          = 0x600b8bc1; /* (AvrekStudio) (base LPSPI, xfer *) : transfert bloquant du pilote NXP ; xfer = {tx, rx (0 : ignoré), taille, drapeaux} ; drapeaux de fw_eve_spi_byte : 0x100000 (PCS continu), PCS 0. fw_eve_spi_byte l'appelle pour UN octet (cause de la lenteur de l'image des invités) */
fw_eve_spi_dev         = 0x20002804; /* (AvrekStudio) pointeur vers l'objet SPI de l'écran ; son premier mot = base du LPSPI3 (0x4039c000) */
fw_power_off           = 0x600aaa0d; /* (AvrekStudio) case 13 (+0x34) de la table de fw_hw (0x6003042c) : écran éteint, LED éteintes, puis relâche la broche qui maintient l'alimentation. L'officiel l'appelle quand le bouton 12 (power) reste tenu 1500 ms (objet d'extinction app + 0x46a0, 0x600ce268) */
fw_codec_hp            = 0x600aa09d; /* (AvrekStudio) (codec, gauche, droite) : écrit CHIP_ANA_HP_CTRL (0x22) du SGTL5000 par I2C, en pas de 0,5 dB (0 = +12 dB, 0x18 = 0 dB). Appelée par le Master de l'officiel (0x600ab904) ; boucle principale seulement (I2C bloquant) */
fw_codec               = 0x20003048; /* (AvrekStudio) objet codec passé à fw_codec_hp (adresse I2C 0x0a en +0xa). Relevé au Play+ virtuel le 26/11 en tournant le Master (cible base) */
fw_sample_pool         = 0x2001b138; /* (AvrekStudio) mémoire des samples (Play+ virtuel, 25/11/2026) : +0 début 0x804d900c, +4 capacité en échantillons int16 (0xd92f3a), +8 curseur présumé (= début, projet vide). Bloc de 0x1b25e74 o alloué au démarrage par 0x600c9ecd dans l'allocateur SDRAM 0x600ab610 (bump, 32 Mo depuis 0x80000000) */
fw_vap_voice_render    = 0x601315e1; /* (AvrekStudio) relais du calcul d'une voix VAP (-> ITCM 0x6c00) : slot 4 des vtables 0x6008baac / 0x6008bae4 ; (objet, r1, r2, r3, s0, s1 = tampon 128 flottants) */
fw_input_listener      = 0x20018c34; /* (AvrekStudio) objet listener des entrées : app 0x20018c18 + 0x1c ; vtable 0x6007b27c ; +8 = UI manager */
fw_in_pad              = 0x600d9cd5; /* input listener vtable 0x6007b27c [0..4] */
fw_in_button           = 0x600d9d95;
fw_in_knob             = 0x600d9d3d;
fw_in_track            = 0x600d9d6d;
fw_in_encoder          = 0x600d9e5d;
fw_cur_param          = 0x2001b05c; /* (AvrekStudio) index du paramètre courant (potard choisi) : contrôleur app 0x20018c18 + 0x38, +0x240c (note FreePlay, vérifié au test vdev_master_shift, 26/11/2026) ; l'encodeur seul règle ce paramètre */
fw_shift_held          = 0x20018c4c; /* (AvrekStudio) octet : Shift tenu, pour l'officiel (app 0x20018c18 + 0x34) ; 1 pendant l'appui, 0 relâché (Play+ virtuel, 26/11/2026). Avec Shift, un potard avance par gros pas (Master Volume : 12 unités = 6 dB par cran au lieu de 1 = 0,5 dB) */
fw_hw_update           = 0x600aab29; /* (AvrekStudio) (fw_hw) : balaie tout le matériel (pads, boutons, potards, toucher, encodeur) et envoie les événements au listener [fw_input_listener] ; appelle aussi l'automate d'affichage en 0x600aab34. Appelé à chaque tour de la boucle principale (bl en 0x600ad85e) */
fw_hw                  = 0x20004c9c; /* (AvrekStudio) objet matériel passé à fw_hw_update par la boucle principale (r7, relevé au virtuel le 26/11/2026) */
fw_bg_services         = 0x600b0f91; /* (AvrekStudio) boucle principale, appel en 0x600ad858 : services de fond ; via 0x600aef60 (objets 0x20005df8 et 0x20005e14) il interroge les deux contrôleurs des PADS en I2C (adresse 0x28, LPI2C1 et LPI2C2) ; aussi 0x20006270 et 0x20003048 (réglages audio) */
fw_bg_service2         = 0x600aacf9; /* (AvrekStudio) boucle principale, appel en 0x600ad864 avec fw_bg_service2_obj */
fw_bg_service2_obj     = 0x200058f0; /* (AvrekStudio) argument (r8) de fw_bg_service2 dans la boucle principale */
fw_app_holder          = 0x20005a18; /* (AvrekStudio) contient le pointeur de l'application (0x20018c18) ; la boucle appelle la case 1 de sa table à chaque tour */
fw_gpio_write          = 0x600b9335; /* (AvrekStudio) (port, broche, valeur) : la boucle principale bascule GPIO3 (0x401c0000) broche 26 toutes les 500 ms */
fw_periodic_5s         = 0x600ae379; /* (AvrekStudio) (fw_periodic_5s_obj) : tâche appelée toutes les 5 s par la boucle principale */
fw_periodic_5s_obj     = 0x20004c90;
fw_millis              = 0x200064c8; /* u32 ms counter (FUN_600b1394) */
fw_pattern_switch      = 0x600db229; /* FUN_600db228(pm, pattern, mode 1 nextstep / 2 trackend, 0) */
fw_menu_build_file     = 0x600e9fd9; /* stock "File" builder FUN_600e9fd8(ctx, menu) */
fw_f_opendir           = 0x600bf1f9; /* FatFs f_opendir(DIR*, path) (wrapper FUN_600aa804) */
fw_f_readdir           = 0x600bf319; /* FatFs f_readdir(DIR*, FILINFO*): fattrib +0xc, fname +0x1a */
fw_f_closedir          = 0x600bf2ad; /* FatFs f_closedir(DIR*) */
fw_rmtree              = 0x600aa6cd; /* FUN_600aa6cc(_, char *path, char *path_end, FILINFO*): recursive delete */
fw_f_mkdir             = 0x600bf491; /* FatFs f_mkdir(path) -> FRESULT (0 ok, 8 exists) */
fw_pool_add_folder     = 0x600ca259; /* (pool, name) -> 0 added, 2 exists, 3 full: pool folder (meta = *(pool+8)) */
fw_pool_find_folder    = 0x600ca3b5; /* (pool, name) -> folder index or -1 */
fw_import_files        = 0x600ccea1; /* (project, dir, char *names[], count, folder) -> 0 ok, 1 no memory, 2 255 samples */
fw_fmt_startend        = 0x600da3dd; /* Sample Start / End descriptors vt[0]: (self, value, mode, buf, len, step-or-track) value text */
fw_led_set             = 0x600ac581; /* (= fw_pad_led_set, nom FreePlay) LED object vt+4: (leds, led, rgb *, rgb2 *, mode) */
fw_led_trig_color      = 0x60078dac; /* u8[3] the stock colour of a plain trig pad (55 77 55) */
fw_step_mode_vt        = 0x60080eb8; /* vtable of the step-grid pad mode */
fw_fx_preset           = 0x600ec9a9; /* (fx, type 0 reverb / 1 delay / 2 limiter, index): the stock preset rows' apply */
fw_fx_saturation       = 0x600ecaad; /* (fx, type): the stock Saturation row's apply */
fw_delay_presets       = 0x6007868c; /* {params *, name *}[20] */
fw_reverb_presets      = 0x6007872c; /* [11] */
fw_limiter_presets     = 0x60078784; /* [18] */
fw_sat_names           = 0x6007ac70; /* const char *[5] */
fw_scale_names         = 0x6007a828; /* const char *[39] */
fw_reader_gain         = 0x60112b75; /* (reader): sets this block's gain from the fade in / out */
fw_tgt_get             = 0x6010ddd9; /* (target) -> current value */
fw_tgt_set             = 0x6010dd81; /* (target, mask, value) */
fw_inst_vt_sample      = 0x600892e4; /* sample instrument vtable */
fw_menu_build_cc       = 0x600e7cf9; /* (ctx, menu, channel): a channel's 8 CC rows (MIDI > CC Mapping) */
fw_step_toggle         = 0x600de6f1; /* (selection, track, step): as a step pad: default step + on, or off */
