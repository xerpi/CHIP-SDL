#include "chip-3ds.h"
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>


#define FONT_OFFSET 0
static const uint8_t chip8_font[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, 0x20, 0x60, 0x20, 0x20, 0x70,
    0xF0, 0x10, 0xF0, 0x80, 0xF0, 0xF0, 0x10, 0xF0, 0x10, 0xF0,
    0x90, 0x90, 0xF0, 0x10, 0x10, 0xF0, 0x80, 0xF0, 0x10, 0xF0,
    0xF0, 0x80, 0xF0, 0x90, 0xF0, 0xF0, 0x10, 0x20, 0x40, 0x40,
    0xF0, 0x90, 0xF0, 0x90, 0xF0, 0xF0, 0x90, 0xF0, 0x10, 0xF0,
    0xF0, 0x90, 0xF0, 0x90, 0x90, 0xE0, 0x90, 0xE0, 0x90, 0xE0,
    0xF0, 0x80, 0x80, 0x80, 0xF0, 0xE0, 0x90, 0x90, 0x90, 0xE0,
    0xF0, 0x80, 0xF0, 0x80, 0xF0, 0xF0, 0x80, 0xF0, 0x80, 0x80
};

void chip3ds_init(struct chip3ds_context *ctx, uint8_t display_w, uint8_t display_h)
{
    ctx->disp_mem = malloc((display_w * display_h)/8);
    ctx->disp_w = display_w;
    ctx->disp_h = display_h;
    chip3ds_reset(ctx);
}

void chip3ds_reset(struct chip3ds_context *ctx)
{
    memset(ctx->RAM,   0, sizeof(ctx->RAM));
    memcpy(ctx->RAM+FONT_OFFSET, chip8_font, sizeof(chip8_font));
    memset(ctx->stack, 0, sizeof(ctx->stack));
    memset(&ctx->regs, 0, sizeof(ctx->regs));
    ctx->regs.PC = 0x200;
    ctx->regs.SP = 0;
    ctx->keyboard = 0;
    chip3ds_cls(ctx);
    ctx->ticks = 0;
};

void chip3ds_fini(struct chip3ds_context *ctx)
{
    if (ctx->disp_mem)
        free(ctx->disp_mem);
}

void chip3ds_step(struct chip3ds_context *ctx)
{
    uint16_t instrBE = ctx->RAM[ctx->regs.PC+1]<<8 | ctx->RAM[ctx->regs.PC];
    uint16_t instr = (instrBE & 0xFF)<<8 | (instrBE>>8);
    ctx->regs.PC+=2;
    
    //printf("instrBE: 0x%04X\ninstr: 0x%04X\n", instrBE, instr);
    
    switch ((instr>>12) & 0xF) {
        case 0:
            switch (instr & 0xFF) {
        /* CLS */
            case 0xE0:
                chip3ds_cls(ctx);
                break;
        /* RET */
            case 0xEE:
                ctx->regs.PC = ctx->stack[ctx->regs.SP];
                ctx->regs.SP--;
                break;
            }
            break;
    /* JP */
        case 1:
            ctx->regs.PC = (instr & 0xFFF);
            break;
    /* CALL */
        case 2:
            ctx->regs.SP++;
            ctx->stack[ctx->regs.SP] = ctx->regs.PC;
            ctx->regs.PC = (instr & 0xFFF);
            break;
    /* SE */
        case 3:
            if (ctx->regs.V[instrBE & 0xF] == (instr & 0xFF)) {
                ctx->regs.PC+=2;
            }
            break;
    /* SNE */
        case 4:
            if (ctx->regs.V[instrBE & 0xF] != (instr & 0xFF)) {
                ctx->regs.PC+=2;
            }
            break;
    /* SE */
        case 5:
            if (ctx->regs.V[instrBE & 0xF] == ctx->regs.V[((instr>>4) & 0xF)]) {
                ctx->regs.PC+=2;
            }
            break;
    /* LD */
        case 6:
            ctx->regs.V[instrBE & 0xF] = instr & 0xFF;
            break;
    /* ADD */
        case 7:
            ctx->regs.V[instrBE & 0xF] += instr & 0xFF;
            break;
        case 8:
            switch (instr & 0xF) {
        /* LD */
            case 0:
                ctx->regs.V[instrBE & 0xF] = ctx->regs.V[(instr>>4) & 0xF];
                break;
        /* OR */
            case 1:
                ctx->regs.V[instrBE & 0xF] |= ctx->regs.V[(instr>>4) & 0xF];
                break;
        /* AND */
            case 2:
                ctx->regs.V[instrBE & 0xF] &= ctx->regs.V[(instr>>4) & 0xF];
                break;
        /* XOR */
            case 3:
                ctx->regs.V[instrBE & 0xF] ^= ctx->regs.V[(instr>>4) & 0xF];
                break;
        /* ADD */
            case 4: {
                uint16_t result = ctx->regs.V[instrBE & 0xF] + ctx->regs.V[(instr>>4) & 0xF];
                ctx->regs.V[instrBE & 0xF] = result & 0xFF;
                ctx->regs.V[0xF] = (result > 0xFF);
                break;
            }
        /* SUB */
            case 5: {
                int8_t result = ctx->regs.V[instrBE & 0xF] - ctx->regs.V[(instr>>4) & 0xF];
                ctx->regs.V[instrBE & 0xF] = result;
                ctx->regs.V[0xF] = (result > 0);
                break;
            }
        /* SHR */
            case 6:
                ctx->regs.V[0xF] = (ctx->regs.V[(instr>>4) & 0xF] & 0x1);
                ctx->regs.V[instrBE & 0xF] = ctx->regs.V[(instr>>4) & 0xF]>>1;
        /* SUBN */
            case 7: {
                int8_t result = ctx->regs.V[(instr>>4) & 0xF] - ctx->regs.V[instrBE & 0xF];
                ctx->regs.V[instrBE & 0xF] = result;
                ctx->regs.V[0xF] = (result > 0);
                break;
            }
        /* SHL */
            case 0xE:
                ctx->regs.V[0xF] = ((ctx->regs.V[(instr>>4) & 0xF]>>7) & 0x1);
                ctx->regs.V[instrBE & 0xF] = ctx->regs.V[(instr>>4) & 0xF]<<1;
                break;
            }
            break;
    /* SNE */
        case 9:
            if (ctx->regs.V[instrBE & 0xF] != ctx->regs.V[(instr>>4) & 0xF]) {
                ctx->regs.PC+=2;
            }
            break;
    /* LD I */
        case 0xA:
            ctx->regs.I = instr & 0xFFF;
            break;
    /* JP V0 */
        case 0xB:
            ctx->regs.PC = ctx->regs.V[0] + (instr & 0xFFF);
            break;
    /* RND */
        case 0xC:
            ctx->regs.V[instrBE & 0xF] = (rand()%256) & (instr&0xFF);
            break;
    /* DRW */
        case 0xD: {
            uint8_t i, n = instr & 0xF,
                x = ctx->regs.V[instrBE & 0xF],
                y = ctx->regs.V[(instr>>4) & 0xF];
            for (i = 0; i < n; i++) {
               //printf("drawing 0x%02X to x: 0x%02X  y: 0x%02X\n",ctx->RAM[ctx->regs.I+i], x/8, y);
                uint8_t disp_idx = x/8 + (ctx->disp_w/8)*(y+i);
                uint16_t RAM_idx = ctx->regs.I+i;
                ctx->regs.V[0xF] |=
                    ((ctx->disp_mem[disp_idx] ^ ctx->RAM[RAM_idx]) & ctx->disp_mem[disp_idx]) ? 0b1 : 0b0;
                ctx->disp_mem[disp_idx] ^= ctx->RAM[RAM_idx];
            }
            break;
        }
        case 0xE:
            switch (instr & 0xFF) {
        /* SKP */
            case 0x9E:
                if ((ctx->keyboard>>ctx->regs.V[instrBE & 0xF]) & 0b1) {
                    ctx->regs.PC+=2;
                }
                break;
        /* SKNP */
            case 0xA1:
                if ((ctx->keyboard>>ctx->regs.V[instrBE & 0xF]) & ~0b1) {
                    ctx->regs.PC+=2;
                }
                break;
            }
            break;
        case 0xF:
            switch (instr & 0xFF) {
        /* LD DT */
            case 0x07:
                ctx->regs.V[instrBE & 0xF] = ctx->regs.DT;
                break;
        /* LD KEY */
            case 0x0A: {
                uint16_t new_pressed = (ctx->keyboard ^ ctx->keyboard) & ctx->keyboard;
                if (new_pressed) {
                    ctx->regs.V[instrBE & 0xF] = ffs(new_pressed);
                } else {
                    ctx->regs.PC-=2;
                }
                break;
            }
        /* LD DT (set) */
            case 0x15:
                ctx->regs.DT = ctx->regs.V[instrBE & 0xF];
                break;
        /* LD ST (set) */
            case 0x18:
                ctx->regs.ST = ctx->regs.V[instrBE & 0xF];
                break;
        /* ADD I */
            case 0x1E:
                ctx->regs.I += ctx->regs.V[instrBE & 0xF];
                break;
        /* LD sprite */
            case 0x29:
                printf("setting font n %i\n", ctx->regs.V[instrBE & 0xF]);
                printf("instrBE: 0x%04X\ninstr: 0x%04X\n", instrBE, instr);
                ctx->regs.I = FONT_OFFSET+ctx->regs.V[instrBE & 0xF]*5;
                break;
        /* LD BCD */
            case 0x33: {
                uint8_t n = ctx->regs.V[instrBE & 0xF];
                ctx->RAM[ctx->regs.I]   = (n/100)%10;
                ctx->RAM[ctx->regs.I+1] = (n/10)%10;
                ctx->RAM[ctx->regs.I+2] = n%10;
                break;
            }
        /* LD mpoke */
            case 0x55: {
                int i;
                for (i = 0; i <= (instrBE & 0xF); i++) {
                    ctx->RAM[ctx->regs.I+i] = ctx->regs.V[i];
                }
                ctx->regs.I += (instrBE & 0xF)+1;
                break;
            }
        /* LD mpeek */
            case 0x65: {
                int i;
                for (i = 0; i <= (instrBE & 0xF); i++) {
                    ctx->regs.V[i] = ctx->RAM[ctx->regs.I+i];
                }
                break;
                ctx->regs.I += (instrBE & 0xF)+1;
            }
            break;
        }
    }
    ctx->ticks++;
    if (ctx->regs.DT > 0) ctx->regs.DT--;
    if (ctx->regs.ST > 0) ctx->regs.ST--;
}


void chip3ds_cls(struct chip3ds_context *ctx)
{
    memset(ctx->disp_mem, 0, (ctx->disp_w * ctx->disp_h)/8);
}

void chip3ds_key_press(struct chip3ds_context *ctx, uint8_t key)
{
    if (key < 16)
        ctx->keyboard |= (1<<key);
}

void chip3ds_key_release(struct chip3ds_context *ctx, uint8_t key)
{
    if (key < 16)
        ctx->keyboard &= ~(1<<key);
}

int chip3ds_loadrom(struct chip3ds_context *ctx, char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    fseek(fp, 0, SEEK_END);
    long int size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    long read = fread(&(ctx->RAM[0x200]), 1, size, fp);
    fclose(fp);
    return (size == read);
}

void chip3ds_core_dump(struct chip3ds_context *ctx)
{
    printf("Registers:\n");
    printf("  V0: 0x%02X  V1: 0x%02X  V2: 0x%02X  V3: 0x%02X\n",
        ctx->regs.V[0], ctx->regs.V[1], ctx->regs.V[2], ctx->regs.V[3]);
    printf("  V4: 0x%02X  V5: 0x%02X  V6: 0x%02X  V7: 0x%02X\n",
        ctx->regs.V[4], ctx->regs.V[5], ctx->regs.V[6], ctx->regs.V[7]);
    printf("  V8: 0x%02X  V9: 0x%02X  VA: 0x%02X  VB: 0x%02X\n",
        ctx->regs.V[8], ctx->regs.V[9], ctx->regs.V[0xA], ctx->regs.V[0xB]);
    printf("  VC: 0x%02X  VD: 0x%02X  VE: 0x%02X  VF: 0x%02X\n",
        ctx->regs.V[0xC], ctx->regs.V[0xD], ctx->regs.V[0xE], ctx->regs.V[0xF]);
    printf("  I: 0x%04X  PC: 0x%04X  SP: 0x%02X  DT: 0x%02X  ST: 0x%02X\n",
        ctx->regs.I, ctx->regs.PC, ctx->regs.SP, ctx->regs.DT, ctx->regs.ST);
    
}
