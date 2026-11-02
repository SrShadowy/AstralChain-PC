#pragma once
// Astral Chain extra Maxwell translator extensions for compute shaders (LEA family).

    } else if (OPM(0x36d0000000000000ull, 0xfef8000000000000ull)) {  // LEA (imm20 form)
        int32_t imm20 = (int32_t)(bits(w, 20, 19) | (bits(w, 56, 1) ? ~0x7ffffu : 0));
        int sh = (int)bits(w, 39, 5);
        bool neg = bits(w, 45, 1);
        sprintf(u, "((%s%s << %du) + %lld)", neg ? "-" : "", R(a), sh, (long long)imm20);
        set_reg(c, d, u);
    } else if (OPM(0x1800000000000000ull, 0xfc00000000000000ull)) {  // LEA.HI (cbuf form)
        char cb[256];
        const char *bv = cbuf_ref(c, (int)bits(w, 34, 5), bits(w, 20, 14) * 4, NULL, cb);
        int hi = (int)bits(w, 39, 8), sh = (int)bits(w, 51, 5);
        bool neg = bits(w, 56, 1);
        if (sh == 0) sprintf(u, "(%s + %s)", bv, R(hi));
        else sprintf(u, "(%s + uint((ulong(%s) << 32 | ulong(%s%s)) >> %du))", bv, R(hi), neg ? "-" : "", R(a), 32 - sh);
        set_reg(c, d, u);
    } else if (OPM(0x4bd0000000000000ull, 0xfff0000000000000ull)) {  // LEA (cbuf form)
        char cb[256];
        const char *bv = cbuf_ref(c, (int)bits(w, 34, 5), bits(w, 20, 14) * 4, NULL, cb);
        int sh = (int)bits(w, 39, 5);
        bool neg = bits(w, 45, 1);
        sprintf(u, "((%s%s << %du) + %s)", neg ? "-" : "", R(a), sh, bv);
        set_reg(c, d, u);
    } else if (OPM(0x5bd0000000000000ull, 0xfff8000000000000ull)) {  // LEA (reg form)
        int b = (int)bits(w, 20, 8), sh = (int)bits(w, 39, 5);
        bool neg = bits(w, 45, 1);
        sprintf(u, "((%s%s << %du) + %s)", neg ? "-" : "", R(a), sh, R(b));
        set_reg(c, d, u);
