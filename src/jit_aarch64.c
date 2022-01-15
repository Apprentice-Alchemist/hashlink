// Reference Manual : https://developer.arm.com/documentation/ddi0487/latest/
// Procedure Call Standard :
// https://github.com/ARM-software/abi-aa/blob/2bcab1e3b22d55170c563c3c7940134089176746/aapcs64/aapcs64.rst
// Apple Silicon
//      https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
//      https://developer.apple.com/documentation/apple-silicon/porting-just-in-time-compilers-to-apple-silicon
// Windows
//      https://docs.microsoft.com/en-us/cpp/build/arm64-windows-abi-conventions
// Android
//      https://developer.android.com/ndk/guides/abis#arm64-v8a

#include "hl.h"
#include <assert.h>
#include <hlmodule.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

// CPU register
#define XREG(i) (i)
// FPU/SIMD register
#define VREG(i) (RCPU_COUNT + i)
// Zero Register
#define ZR REG_COUNT
// Stack Pointer
#define SP REG_COUNT

#define RCPU_COUNT 31
#define RFPU_COUNT 32
// 31 general purpose register + 32 fpu register + sp/zr special register
#define REG_COUNT RCPU_COUNT + RFPU_COUNT + 1

#define REG_AT(i) ctx->pregs + (i)
#define R(i) ctx->vregs + i

#define BUF_POS() (intptr_t)((unsigned char *)ctx->buf - ctx->startBuf)

#define T_IS_FLOAT(type) ((type == HF32) || (type == HF64))
#define T_IS_64(type)                                                          \
	!((type == HUI8) || (type == HUI16) || (type == HI32) || (type == HF32))
#define MAX_OP_SIZE 256

#define ERROR(fmt, ...)                                                        \
	{                                                                          \
		printf("JIT ERROR : " fmt "\n", ##__VA_ARGS__);                        \
		fflush(stdout);                                                        \
		hl_debug_break();                                                      \
		abort();                                                               \
	}

#define ASSERT(i)                                                              \
	{                                                                          \
		printf("JIT ASSERT %i (line %i)\n", i, __LINE__);                      \
		fflush(stdout);                                                        \
		__builtin_trap();                                                      \
		abort();                                                               \
	}

#define TODO(fmt, ...)                                                         \
	{                                                                          \
		printf("TODO : " fmt "\n", ##__VA_ARGS__);                             \
		fflush(stdout);                                                        \
		__builtin_trap();                                                      \
		abort();                                                               \
	}

typedef enum CpuOp {
	ADR,
	ADRP,

	ADD,
	ADDS,
	SUB,
	SUBS,

	AND,
	ORR,
	EOR,
	ANDS,

	ORN,
	EON,
	BIC,
	BICS,

	ADC,
	ADCS,
	SBC,
	SBCS,

	MOVN,
	MOVZ,
	MOVK,

	SBFM,
	BFM,
	UBFM,

	EXTR,

	BCOND,

	BR,
	BLR,
	RET,

	B,
	BL,

	CBZ,
	CBNZ,

	TBZ,
	TBNZ,

	LDR,
	LDRB,
	LDRSB,
	LDRH,
	LDRSH,
	SDR,
	SDRB,
	SDRSB,
	SDRH,
	SDRSH,

	LSLV,
	LSRV,
	ASRV,
	RORV,
	UDIV,
	SDIV,

	RBIT,
	REV16,
	REV,
} CpuOp;

typedef enum FpuOp {
	FMOV,
	FABS,
	FNEG,
	FSQRT,
	FCVT,
	FCMP,
	FCMPE,
	FMUL,
	FDIV,
	FADD,
	FSUB,
	FMAX,
	FMIN
} FpuOp;

typedef enum CondCode {
	// meaning (integer) / meaning(floating point)
	EQ = 0,  // equal / equal
	NE = 1,  // not equal / not equal or unordered
	CS = 2,  // carry set / greater than, equal, or unordered
	HS = CS, // carry clear / less than
	CC = 3,  // minus, negative / less than
	LO = CC, // plus, positive or zero / greater than, equal, or unordered
	MI = 4,  // overflow / unordered
	PL = 5,  // no overflow / ordered
	VS = 6,  // unsigned higher / greater than, or unordered
	VC = 7,  // unsigned lower or same / less than or equal
	HI = 8,  // signed greater than or equal / greater than or equal
	LS = 9,  // unsigned lower or same / less than or equal
	GE = 10, // signed greater than or equal / greater than or equal
	LT = 11, // signed less than / less than, or unordered
	GT = 12, // signed greater than / greater than
	LE = 13, // signed less than / less than, equal, or unordered
	AL = 14, // always / always
	NV = 15  // always / always
} CondCode;

typedef enum BarrierType { CLREX, DSB, DMB, ISB, SB } BarrierType;

typedef enum BarrierOption {
	SY = 15,
	ST = 14,
	LD = 13,
	ISH = 11,
	ISHST = 10,
	ISHLD = 9,
	NSH = 7,
	NSHST = 6,
	NSHLD = 5,
	OSH = 3,
	OSHST = 2,
	OSHLD = 1
} BarrierOption;

typedef struct jlist jlist;
struct jlist {
	CpuOp op;
	int pos;
	int target;
	jlist *next;
};

typedef struct vreg vreg;

typedef enum preg_kind { RCPU = 0, RFPU = 1 } preg_kind;

typedef struct preg {
	preg_kind kind;
	int id;
	int lock;
	vreg *holds;
} preg;

struct vreg {
	int stackPos;
	int size;
	hl_type *t;
	preg *current;
};

struct jit_ctx {
	uint32_t *buf;
	vreg *vregs;
	preg pregs[REG_COUNT];
	vreg *savedRegs[REG_COUNT];
	int savedLocks[REG_COUNT];
	int *opsPos;
	int maxRegs;
	int maxOps;
	size_t bufSize;
	int totalRegsSize;
	int functionPos;
	int allocOffset;
	int currentPos;
	int nativeArgsCount;
	unsigned char *startBuf;
	hl_module *m;
	hl_function *f;
	jlist *jumps;
	jlist *calls;
	jlist *switchs;
	hl_alloc falloc; // cleared per-function
	hl_alloc galloc;
	vclosure *closure_list;
	hl_debug_infos *debug;
	int c2hl;
	int hl2c;
	int longjump;
	void *static_functions[8];
	bool calling;
};

// static preg _unused = {RUNUSED};
// #define UNUSED &_unused

static void jit_buf(jit_ctx *ctx) {
	if (BUF_POS() + MAX_OP_SIZE > ctx->bufSize) {
		size_t nsize = ctx->bufSize * 4 / 3;
		unsigned char *nbuf;
		size_t curpos;
		if (nsize == 0) {
			int i;
			for (i = 0; i < ctx->m->code->nfunctions; i++)
				nsize += ctx->m->code->functions[i].nops;
			nsize *= 4;
		}
		if (nsize < ctx->bufSize + MAX_OP_SIZE * 4)
			nsize = ctx->bufSize + MAX_OP_SIZE * 4;
		curpos = BUF_POS();
		nbuf = (unsigned char *)malloc(nsize);
		if (nbuf == NULL)
			ERROR("Failed to allocate jit buffer, size=%lu.", nsize);
		if (ctx->startBuf) {
			memcpy(nbuf, ctx->startBuf, curpos);
			free(ctx->startBuf);
		}
		ctx->startBuf = nbuf;
		assert(ctx->startBuf != NULL);
		ctx->buf = (uint32_t *)(nbuf + curpos);
		ctx->bufSize = nsize;
	}
}

static int type_stack_size(hl_type *t) {
	switch (t->kind) {
	case HUI8:
	case HUI16:
	case HBOOL:
	case HI32:
	case HF32:
		return sizeof(intptr_t);
	default:
		return hl_type_size(t);
	}
}

#define W(v) *ctx->buf++ = v

static void emit_adr(jit_ctx *ctx, CpuOp cop, uint64_t imm, preg *d) {
	uint32_t op;
	switch (cop) {
	case ADR:
		op = 0;
		break;
	case ADRP:
		op = 1;
		break;
	default:
		ASSERT(cop);
	}
	// https://github.com/bytecodealliance/wasmtime/blob/3ba9e5865a8171d1b4547bcabe525666d030c18b/cranelift/codegen/src/isa/aarch64/inst/emit.rs#L333
	uint32_t immlo = imm & 3;
	uint32_t immhi = (imm >> 2) & ((1 << 19) - 1);
	W(0x10000000 | (op << 31) | ((immhi & 0x7FFFF) << 5) |
	  ((immlo & 0x3) << 29) | (d->id & 0x1F));
}

static void emit_ari_imm(jit_ctx *ctx, CpuOp cop, bool is64, int32_t imm,
                         preg *n, preg *d) {
	uint32_t sf = is64 ? 1 : 0;
	uint32_t op;
	uint32_t S;
	uint32_t sh = 0;

	switch (cop) {
	case ADD:
		op = 0;
		S = 0;
		break;
	case ADDS:
		op = 0;
		S = 1;
		break;
	case SUB:
		op = 1;
		S = 0;
		break;
	case SUBS:
		op = 1;
		S = 1;
		break;
	default:
		ASSERT(cop);
	}
	W(0x11000000 | (sf << 31) | (op << 30) | S << 29 | (sh << 22) |
	  ((imm & 0xFFF) << 10) | ((n->id & 0x1F) << 5) | (d->id & 0x1F));
}

// static void emit_log_imm(jit_ctx *ctx, CpuOp cop, bool is64, uint32_t imm,
// preg *n, preg *d)
// {
//     uint32_t sf = is64 ? 1 : 0;
//     uint32_t opc;
//     uint32_t N = 0;
//     switch (cop)
//     {
//     case AND:
//         opc = 0;
//         break;
//     case ORR:
//         opc = 1;
//         break;
//     case EOR:
//         opc = 2;
//         break;
//     case ANDS:
//         opc = 3;
//         break;
//     default:
//         ASSERT(cop);
//     }
//     TODO("Deal with logical immediates.");
// W(0x12000000 | (sf << 31) | (opc << 29) | (N << 22) | (imm));
// }

static void emit_movw_imm(jit_ctx *ctx, CpuOp cop, bool is64, int64_t imm,
                          uint32_t shift, preg *d) {
	uint32_t sf = is64 ? 1 : 0;
	uint32_t opc;
	uint32_t hw = shift / 16;

	switch (cop) {
	case MOVN:
		opc = 0;
		break;
	case MOVZ:
		opc = 2;
		break;
	case MOVK:
		opc = 3;
		break;
	default:
		ASSERT(cop);
	}
	W(0x12800000 | (sf << 31) | (opc << 29) | (hw << 21) |
	  ((imm & 0xFFFF) << 5) | (d->id & 0x1F));
}

static void emit_cond_branch(jit_ctx *ctx, CondCode cond, uint32_t imm) {
	W(0x54000000 | ((imm & 0x7FFFF) << 5) | cond);
}

static void emit_brk(jit_ctx *ctx, uint16_t imm) { W(0xD4200000 | (imm << 5)); }

static void emit_barrier(jit_ctx *ctx, BarrierType type, BarrierOption opt) {
	uint32_t CRm = opt;
	uint32_t op2;
	uint32_t Rt = 15;
	switch (type) {
	case CLREX:
		op2 = 2;
		break;
	case DSB:
		op2 = 4;
		break;
	case DMB:
		op2 = 5;
		break;
	case ISB:
		op2 = 6;
		break;
	case SB:
		op2 = 7;
		break;
	}
	W(0xD5033000 | (CRm << 8) | (op2 << 5) | Rt);
}

static void emit_uncond_branch_reg(jit_ctx *ctx, CpuOp cop, preg *n) {
	switch (cop) {
	case BR:
		W(0xD61F0000 | ((n->id & 0x1F) << 5));
		break;
	case BLR:
		W(0xD63F0000 | ((n->id & 0x1F) << 5));
		break;
	case RET:
		W(0xD65F0000 | ((n->id) << 5));
		break;
	default:
		ASSERT(cop);
	}
}

static void emit_uncond_branch_imm(jit_ctx *ctx, CpuOp cop, uint64_t imm) {
	imm &= 0x3FFFFFF;
	switch (cop) {
	case B:
		W(0x14000000 | imm);
		break;
	case BL:
		W(0x94000000 | imm);
		break;
	default:
		ASSERT(cop);
	}
}

// static void emit_comp_and_branch_imm(jit_ctx *ctx) {}
// static void emit_test_and_branch_imm(jit_ctx *ctx) {}

static void emit_data_proc_rrr(jit_ctx *ctx, CpuOp cop, bool is64, preg *m,
                               preg *n, preg *d) {
	uint32_t sf = is64 ? 1 : 0;
	uint32_t S;
	uint32_t opcode;
	switch (cop) {
	case UDIV:
		S = 0;
		opcode = 2;
		break;
	case SDIV:
		S = 0;
		opcode = 3;
		break;
	case LSLV:
		S = 0;
		opcode = 8;
		break;
	case LSRV:
		S = 0;
		opcode = 9;
		break;
	case ASRV:
		S = 0;
		opcode = 10;
		break;
	case RORV:
		S = 0;
		opcode = 11;
		break;
	default:
		ASSERT(cop);
	}

	W(0x1AC00000 | (sf << 31) | (S << 29) | ((m->id & 0x1F) << 16) |
	  (opcode << 10) | ((n->id & 0x1F) << 5) | (d->id & 0x1F));
}
static void emit_data_proc_rr(jit_ctx *ctx, CpuOp cop, bool is64) {}

typedef enum ShiftType { LSL = 0, LSR = 1, ASR = 2, ROR = 3 } ShiftType;

static void emit_log_shift_reg(jit_ctx *ctx, CpuOp cop, bool is64, preg *m,
                               preg *n, preg *d, ShiftType shift,
                               uint32_t amount) {
	uint32_t sf = is64 ? 1 : 0;
	uint32_t opc;
	uint32_t N;
	uint32_t imm = amount;

	switch (cop) {
	case AND:
		opc = 0;
		N = 0;
		break;
	case BIC:
		opc = 0;
		N = 1;
		break;
	case ORR:
		opc = 1;
		N = 0;
		break;
	case ORN:
		opc = 1;
		N = 1;
		break;
	case EOR:
		opc = 2;
		N = 0;
		break;
	case EON:
		opc = 2;
		N = 1;
		break;
	case ANDS:
		opc = 3;
		N = 0;
		break;
	case BICS:
		opc = 3;
		N = 1;
		break;
	default:
		ASSERT(cop);
	}

	W(0x0A000000 | (sf << 31) | (opc << 29) | (shift << 22) | (N << 21) |
	  (m->id << 16) | (imm << 10) | (n->id << 5) | d->id);
}

static void emit_ari_shift_reg(jit_ctx *ctx, CpuOp cop, bool is64, preg *m,
                               preg *n, preg *d, ShiftType shift,
                               uint32_t amount) {
	uint32_t sf = is64 ? 1 : 0;
	uint32_t op;
	uint32_t S;
	uint32_t imm = amount;

	switch (cop) {
	case ADD:
		op = 0;
		S = 0;
		break;
	case ADDS:
		op = 0;
		S = 1;
		break;
	case SUB:
		op = 1;
		S = 0;
		break;
	case SUBS:
		op = 1;
		S = 1;
		break;
	default:
		ASSERT(cop);
	}
	W(0x0B000000 | (sf << 31) | (op << 30) | (shift << 22) | (S << 29) |
	  (m->id << 16) | (imm << 10) | (n->id << 5) | d->id);
}

static void emit_ari_ext_reg(jit_ctx *ctx, CpuOp cop, bool is64) {}
static void emit_ari_carry(jit_ctx *ctx, CpuOp cop, bool is64) {}
static void emit_cond_comp_reg(jit_ctx *ctx, CpuOp cop, bool is64) {}
static void emit_cond_comp_imm(jit_ctx *ctx, CpuOp cop, bool is64) {}

static void emit_float_comp(jit_ctx *ctx, FpuOp cop, bool is64) {}
static void emit_float_imm(jit_ctx *ctx, FpuOp cop, bool is64) {}
static void emit_float_cond_comp(jit_ctx *ctx, FpuOp cop, bool is64) {}
static void emit_float_data_proc_rrr(jit_ctx *ctx, FpuOp cop, bool is64) {}

static void emit_mov_rr(jit_ctx *ctx, bool is64, preg *r, preg *d) {
	if (r->id == 31 || d->id == SP) {
		emit_ari_imm(ctx, ADD, true, 0, r, d);
	} else {
		emit_log_shift_reg(ctx, ORR, is64, r, REG_AT(ZR), d, LSL, 0);
	}
}

static uint32_t type_to_size(hl_type_kind t) {
	switch (t) {
	case HVOID:
		ERROR("Did not expect HVOID");
	case HUI8:
		return 0;
	case HUI16:
		return 1;
	case HF32:
	case HI32:
		return 2;
	case HF64:
	case HI64:
	case HBYTES:
	case HDYN:
	case HFUN:
	case HOBJ:
	case HARRAY:
	case HTYPE:
	case HREF:
	case HVIRTUAL:
	case HDYNOBJ:
	case HABSTRACT:
	case HENUM:
	case HNULL:
	case HMETHOD:
	case HSTRUCT:
		return 3;
	case HBOOL:
		switch (sizeof(bool)) {
		case 1:
			return 0;
		case 4:
			return 2;
		default:
			ERROR("Did not expect this weirdly sized boolean.");
		}
	default:
		ERROR("Unhandled type.");
	}
}

static void emit_ldr(jit_ctx *ctx, hl_type_kind type, int32_t offset, preg *r,
                     preg *d) {
	if (type == HVOID)
		return;
	uint32_t size = type_to_size(type);
	uint32_t V = (type == HF32 || type == HF64) ? 1 : 0;
	uint32_t opc;
	if (type == HF32 || type == HF64) {
		opc = 1;
	} else {
		bool s = false; // ype == HI32 || type == HI64;
		uint32_t bits = 1;
		// switch (type) {
		// case HUI8:
		// case HUI16:
		// case HI32:
		//   bits = 0;
		//   break;
		// default:
		//   bits = 1;
		//   break;
		// }
		opc = (s ? 1 : 0) << 1 | bits;
	}
	assert(opc != 0);
	// struct {
	//   int offset:9;
	// } off = {0};
	// off.offset = offset;
	W(0x38000000 | (size << 30) | (V << 26) | (opc << 22) |
	  ((offset & 0x1FF) << 12) | (d->id << 5) | (r->id));
}

static void emit_str(jit_ctx *ctx, hl_type_kind type, int offset,
                     bool postindex, bool wback, preg *r, preg *d) {
	if (type == HVOID)
		return;
	uint32_t size = type_to_size(type);
	uint32_t V = (type == HF32 || type == HF64) ? 1 : 0;
	uint32_t opc = 0;

	if (true) {
		// emit STUR
		W(0x38000000 | (size << 30) | (V << 26) | (opc << 22) |
		  ((offset & 0x1FF) << 12) | (d->id << 5) | (r->id));
	} else {
		W(0x38200800 | (size << 30) | (V << 26) | (opc << 22) |
		  ((offset & 0xFFF) << 12) | (!postindex << 11) | wback << 10 |
		  (d->id << 5) | (r->id));
	}
}

static void emit_nop(jit_ctx *ctx) { W(0xD503201F); }

static void jit_nops(jit_ctx *ctx) {
	while (BUF_POS() & 15)
		emit_nop(ctx);
}

static bool is_call_reg(preg *p) { return p->id < 8; }
static void scratch(jit_ctx *ctx, preg *p, bool release);
static preg *alloc_register(jit_ctx *ctx, preg_kind kind) {
	const int count = (kind == RFPU) ? RFPU_COUNT : RCPU_COUNT;
	preg *oldest = NULL;
	int oldest_age = ctx->currentPos;
	for (int i = 0; i < count; i++) {
		preg *p = REG_AT(kind == RFPU ? VREG(i) : XREG(i));
		if (p->lock >= ctx->currentPos)
			continue;
		if (ctx->calling && is_call_reg(p))
			continue;
		if (p->holds == NULL) {
			p->lock = ctx->currentPos;
			return p;
		} else {
			if (p->lock < oldest_age) {
				oldest_age = p->lock;
				oldest = p;
			}
		}
	}
	assert(oldest != NULL);
	oldest->lock = ctx->currentPos;
	scratch(ctx, oldest, true);
	return oldest;
}
// <param name="andLoad">wether to load from the stack if needed</param>
static preg *fetch(jit_ctx *ctx, vreg *r, bool andLoad) {
	if (r->current)
		return r->current;
	else {
		r->current = alloc_register(ctx, T_IS_FLOAT(r->t->kind) ? RFPU : RCPU);
		r->current->holds = r;
		if (andLoad)
			emit_ldr(ctx, r->t->kind, r->stackPos, r->current, REG_AT(29));
		return r->current;
	}
}

static void load(jit_ctx *ctx, vreg *r, preg *into) {
	if (r->current) {
		if (r->current != into)
			emit_mov_rr(ctx, T_IS_64(r->t->kind), r->current, into);
	} else {
		emit_ldr(ctx, r->t->kind, r->stackPos, into, REG_AT(29));
	}
}

static void bind(vreg *r, preg *p) {
	if (r->current)
		r->current->holds = NULL;
	p->holds = r;
	r->current = p;
}

static void scratch(jit_ctx *ctx, preg *p, bool release) {
	if (p->holds != NULL) {
		emit_str(ctx, p->holds->t->kind, p->holds->stackPos, true, false, p,
		         REG_AT(29));
		if (release) {
			p->holds->current = NULL;
			p->holds = NULL;
		}
	}
}

static void unbind(preg *p) {
	if (p->holds) {
		p->holds->current = NULL;
		p->holds = NULL;
	}
}

static void load_const(jit_ctx *ctx, preg *p, uint32_t size, uint64_t value) {
	emit_movw_imm(ctx, MOVZ, size > 4, value & 0xFFFF, 0, p);
	emit_movw_imm(ctx, MOVK, size > 4, (value >> 16) & 0xFFFF, 16, p);
	if (size > 4) {
		emit_movw_imm(ctx, MOVK, size > 4, (value >> 32) & 0xFFFF, 32, p);
		emit_movw_imm(ctx, MOVK, size > 4, (value >> 48) & 0xFFFF, 48, p);
	}
}

static void mov(jit_ctx *ctx, vreg *src, vreg *dst) {
	preg *s = fetch(ctx, src, true);
	if (dst->current != NULL) {
		emit_mov_rr(ctx, src->size == 8, s, dst->current);
	} else {
		emit_str(ctx, src->t->kind, dst->stackPos, false, false, src->current,
		         REG_AT(29));
	}
}

static void register_jump(jit_ctx *ctx, CpuOp op, size_t pos, int target) {
	jlist *j = (jlist *)hl_malloc(&ctx->falloc, sizeof(jlist));
	j->op = op;
	j->pos = pos;
	j->target = target;
	j->next = ctx->jumps;
	ctx->jumps = j;
	if (target != 0 && ctx->opsPos[target] == 0)
		ctx->opsPos[target] = -1;
}

static uint32_t pass_parameters(jit_ctx *ctx, int arg_count, int *args) {
	uint32_t NGRN = 0;
	uint32_t NSRN = 0;
	uint32_t NPRN = 0;
	uint32_t NSAA = 0;

	for (int i = 0; i < arg_count; i++) {
		vreg *r = R(args[i]);
		if (T_IS_FLOAT(r->t->kind)) {
			if (NSRN < 8) {
				load(ctx, r, REG_AT(VREG(NSRN)));
				NSRN++;
			} else {
				preg *temp = REG_AT(VREG(8));
				emit_str(ctx, r->t->kind, -NSAA, false, true, temp, REG_AT(SP));
				NSAA += type_stack_size(r->t);
			}
		} else {
			if (NGRN < 8) {
				load(ctx, r, REG_AT(NGRN));
				NGRN++;
			} else {
				preg *temp = REG_AT(17);
				emit_str(ctx, r->t->kind, -NSAA, false, true, temp, REG_AT(SP));
				NSAA += type_stack_size(r->t);
			}
		}
	}
	return NSAA;
}

static void start_call(jit_ctx *ctx) {
	ctx->calling = true;
	// save registers
	for (int i = 0; i < 18; i++) {
		scratch(ctx, REG_AT(i), true);
	}
	// hashlink doesn't use the upper 64 bits of fp registers yet, so this is
	// correct, but may have to change if simd gets implemented
	for (int i = 0; i < 8; i++) {
		scratch(ctx, REG_AT(VREG(i)), true);
	}
	for (int i = 16; i < 32; i++) {
		scratch(ctx, REG_AT(VREG(i)), true);
	}
}

static void end_call(jit_ctx *ctx, int stack_size) {
	if (stack_size > 0)
		emit_ari_imm(ctx, ADD, true, stack_size, REG_AT(SP), REG_AT(SP));
	ctx->calling = false;
}
// R30 is the link register
// R29 is the frame pointer
// R19...R28 are callee saved
// R18 is the platform register
// R17 is IP1, a intra-procedure-call temporary register
// R16 is IPO, a intra-procedure-call temporary register
// R9...R15 are temporary registers, caller saved
// R8 i the indirect result location register
// R0...R7 are the paramter registers, with r0 being the result register too
// for floating point registers, the bottom 64 bits of v8...v15 should be saved
static void call(jit_ctx *ctx, vreg *dst, int findex, int arg_count,
                 int *args) {
	start_call(ctx);
	uint32_t stack_size = pass_parameters(ctx, arg_count, args);
	int fid = ctx->m->functions_indexes[findex];
	if (fid >= ctx->m->code->nfunctions) {
		// native function
		load_const(ctx, REG_AT(17), sizeof(void *),
		           (intptr_t)ctx->m->functions_ptrs[findex]);
		emit_uncond_branch_reg(ctx, BLR, REG_AT(17));
	} else {
		if (ctx->m->functions_ptrs[findex]) {
			// already compiled
			emit_uncond_branch_imm(
			    ctx, BL,
			    -(int64_t)((intptr_t)BUF_POS() -
			               (intptr_t)ctx->m->functions_ptrs[findex]) /
			        4);
		} else if (ctx->m->code->functions + fid == ctx->f) {
			// our current function
			emit_uncond_branch_imm(ctx, BL,
			                       -(BUF_POS() - ctx->functionPos) / 4);
		} else {
			// stage for later
			jlist *j = (jlist *)hl_malloc(&ctx->galloc, sizeof(jlist));
			j->pos = BUF_POS();
			j->target = findex;
			j->next = ctx->calls;
			ctx->calls = j;
			emit_uncond_branch_imm(ctx, BL, 0);
		}
	}
	if (dst && dst->t->kind != HVOID)
		bind(dst, REG_AT(0));
	end_call(ctx, stack_size);
}

static void call_reg(jit_ctx *ctx, vreg *dst, preg *fn_adr, int arg_count,
                     int *args) {
	start_call(ctx);
	uint32_t stack_size = pass_parameters(ctx, arg_count, args);
	emit_uncond_branch_reg(ctx, BLR, fn_adr);
	if (dst && dst->t->kind != HVOID)
		bind(dst, REG_AT(0));
	end_call(ctx, stack_size);
}

static void call_native_consts(jit_ctx *ctx, vreg *dst, intptr_t fn_adr,
                               int arg_count, intptr_t *args) {
	start_call(ctx);

	assert(arg_count < 7);
	for (int i = 0; i < arg_count; i++) {
		preg *p = REG_AT(i);
		load_const(ctx, p, sizeof(void *), args[i]);
	}
	load_const(ctx, REG_AT(17), sizeof(void *), fn_adr);
	emit_uncond_branch_reg(ctx, BLR, REG_AT(17));
	if (dst && dst->t->kind != HVOID)
		bind(dst, REG_AT(0));
	end_call(ctx, 0);
}

static void *get_dyncast(hl_type *t) {
	switch (t->kind) {
	case HF32:
		return hl_dyn_castf;
	case HF64:
		return hl_dyn_castd;
	case HI32:
	case HUI16:
	case HUI8:
	case HBOOL:
		return hl_dyn_casti;
	default:
		return hl_dyn_castp;
	}
}

static void make_dyn_cast(jit_ctx *ctx, vreg *dst, vreg *v) {
	start_call(ctx);
	if (v->stackPos >= 0) {
		emit_ari_imm(ctx, ADD, true, v->stackPos, REG_AT(ZR), REG_AT(0));
	} else {
		emit_ari_imm(ctx, SUB, true, v->stackPos, REG_AT(ZR), REG_AT(0));
	}
	load_const(ctx, REG_AT(1), sizeof(hl_type *), (int_val)v->t);
	if (!T_IS_FLOAT(dst->t->kind)) {
		load_const(ctx, REG_AT(2), sizeof(hl_type *), (int_val)dst->t);
	}
	load_const(ctx, REG_AT(17), sizeof(void *), (int_val)get_dyncast(dst->t));
	emit_uncond_branch_reg(ctx, BLR, REG_AT(17));
	end_call(ctx, 0);
}

int hl_jit_function(jit_ctx *ctx, hl_module *m, hl_function *f) {
	size_t codePos = BUF_POS();

	int nargs = f->type->fun->nargs;
	unsigned short *debug16 = NULL;
	int *debug32 = NULL;
	ctx->f = f;
	ctx->allocOffset = 0;
	if (f->nregs > ctx->maxRegs) {
		free(ctx->vregs);
		ctx->vregs = (vreg *)malloc(sizeof(vreg) * (f->nregs + 1));
		if (ctx->vregs == NULL) {
			ctx->maxRegs = 0;
			return -1;
		}
		ctx->maxRegs = f->nregs;
	}
	memset(ctx->vregs, 0, sizeof(vreg) * (f->nregs + 1));
	if (f->nops > ctx->maxOps) {
		free(ctx->opsPos);
		ctx->opsPos = (int *)malloc(sizeof(int) * (f->nops + 1));
		if (ctx->opsPos == NULL) {
			ctx->maxOps = 0;
			return -1;
		}
		ctx->maxOps = f->nops;
	}
	memset(ctx->opsPos, 0, (f->nops + 1) * sizeof(int));
	for (int i = 0; i < f->nregs; i++) {
		vreg *r = R(i);
		r->t = f->regs[i];
		r->size = hl_type_size(r->t);
		r->current = NULL;
	}
	int size = 0;
	int argsSize = 0;
	for (int i = 0; i < nargs; i++) {
		vreg *r = ctx->vregs + i;
		// args 1 to 7 go in registers
		if (i > 7) {
			// use existing stack storage
			r->stackPos = (argsSize + sizeof(void *) * 2);
			argsSize += type_stack_size(r->t);
		} else {
			// make room in local vars
			size += r->size;
			size += hl_pad_size(size, r->t);
			r->stackPos = -size;
		}
	}
	for (int i = nargs; i < f->nregs; i++) {
		vreg *r = ctx->vregs + i;
		size += r->size;
		size += hl_pad_size(size, r->t); // align local vars
		r->stackPos = -size;
	}
	size += (-size) & 15; // align on 16 bytes
	ctx->totalRegsSize = size;
	jit_buf(ctx);
	ctx->functionPos = BUF_POS();
	assert(ctx->startBuf != NULL && ctx->bufSize != 0);
	// stp x29, x30, [sp, #-16]!
	W(0xA9BF7BFD);
	// mov x29, sp
	emit_mov_rr(ctx, true, REG_AT(SP), REG_AT(29));
	if (ctx->totalRegsSize)
		emit_ari_imm(ctx, SUB, true, ctx->totalRegsSize, REG_AT(SP),
		             REG_AT(SP));
	{
		for (int i = 0; i < nargs; i++) {
			if (i > 7)
				break;
			vreg *r = R(i);
			bind(r, REG_AT(i));
		}
	}

	if (ctx->m->code->hasdebug) {
		debug16 =
		    (unsigned short *)malloc(sizeof(unsigned short) * (f->nops + 1));
		debug16[0] = (unsigned short)(BUF_POS() - codePos);
	}
	ctx->opsPos[0] = BUF_POS();

	for (int opCount = 0; opCount < f->nops; opCount++) {
		jit_buf(ctx);
		ctx->currentPos = opCount + 1;
		hl_opcode *o = &f->ops[opCount];
		vreg *dst = R(o->p1);
		vreg *ra = R(o->p2);
		vreg *rb = R(o->p3);
		switch (o->op) {
		case OMov: {
			vreg *dst = R(o->p1);
			vreg *src = R(o->p2);
			mov(ctx, src, dst);
			break;
		}
		case OInt:
			load_const(ctx, fetch(ctx, R(o->p1), false), sizeof(int), o->p2);
			break;
		case OFloat:
			break;
		case OBool: {
			preg *dst = fetch(ctx, R(o->p1), false);
			emit_ari_imm(ctx, ADD, false, o->p2, REG_AT(ZR), dst);
			break;
		}
		case OBytes: {
			intptr_t b =
			    (intptr_t)(m->code->version >= 5
			                   ? m->code->bytes + m->code->bytes_pos[o->p2]
			                   : m->code->strings[o->p2]);
			preg *dst = fetch(ctx, R(o->p1), false);
			load_const(ctx, dst, sizeof(void *), b);
			break;
		}
		case OString: {
			intptr_t s = (intptr_t)hl_get_ustring(m->code, o->p2);
			preg *dst = fetch(ctx, R(o->p1), false);
			load_const(ctx, dst, sizeof(vbyte *), s);
			break;
		}
		case ONull: {
			preg *dst = fetch(ctx, R(o->p1), false);
			emit_log_shift_reg(ctx, ORR, true, REG_AT(ZR), REG_AT(ZR), dst, LSL,
			                   0);
			break;
		}

		case OAdd:
			emit_brk(ctx, o->op);
			break;
		case OSub:
			emit_brk(ctx, o->op);
			break;
		case OMul:
			emit_brk(ctx, o->op);
			break;
		case OSDiv:
			emit_brk(ctx, o->op);
			break;
		case OUDiv:
			emit_brk(ctx, o->op);
			break;
		case OSMod:
			emit_brk(ctx, o->op);
			break;
		case OUMod:
			emit_brk(ctx, o->op);
			break;
		case OShl:
			emit_brk(ctx, o->op);
			break;
		case OSShr:
			emit_brk(ctx, o->op);
			break;
		case OUShr:
			emit_brk(ctx, o->op);
			break;
		case OAnd:
			emit_brk(ctx, o->op);
			break;
		case OOr:
			emit_brk(ctx, o->op);
			break;
		case OXor:
			emit_brk(ctx, o->op);
			break;

		case ONeg:
			if (T_IS_FLOAT(ra->t->kind)) {
				emit_brk(ctx, o->op);
			} else {
				emit_ari_shift_reg(ctx, SUB, ra->t->kind == HI32,
				                   fetch(ctx, ra, true), REG_AT(ZR),
				                   fetch(ctx, dst, false), LSL, 0);
			}
			break;
		case ONot:
			emit_log_shift_reg(ctx, ORN, ra->t->kind == HI32,
			                   fetch(ctx, ra, true), REG_AT(ZR),
			                   fetch(ctx, dst, false), LSL, 0);
			break;
		case OIncr:
			emit_ari_imm(ctx, ADD, false, 1, fetch(ctx, ra, true),
			             fetch(ctx, dst, false));
			break;
		case ODecr:
			emit_ari_imm(ctx, SUB, false, 1, fetch(ctx, ra, true),
			             fetch(ctx, dst, false));
			break;

		case OCall0:
			call(ctx, R(o->p1), o->p2, 0, NULL);
			break;
		case OCall1:
			call(ctx, R(o->p1), o->p2, 1, &o->p3);
			break;
		case OCall2: {
			int args[2] = {o->p3, (int)(intptr_t)o->extra};
			call(ctx, R(o->p1), o->p2, 2, args);
		} break;
		case OCall3: {
			int args[3] = {o->p3, o->extra[0], o->extra[1]};
			call(ctx, R(o->p1), o->p2, 3, args);
		} break;
		case OCall4: {
			int args[4] = {o->p3, o->extra[0], o->extra[1], o->extra[2]};
			call(ctx, R(o->p1), o->p2, 4, args);
		} break;
		case OCallN:
			call(ctx, R(o->p1), o->p2, o->p3, o->extra);
			break;
		case OCallMethod:
			emit_brk(ctx, o->op);
			break;
		case OCallThis:
			emit_brk(ctx, o->op);
			break;
		case OCallClosure:
			emit_brk(ctx, o->op);
			break;

		case OStaticClosure:
			emit_brk(ctx, o->op);
			break;
		case OInstanceClosure:
			emit_brk(ctx, o->op);
			break;
		case OVirtualClosure:
			emit_brk(ctx, o->op);
			break;

		case OGetGlobal: {
			preg *dst = fetch(ctx, R(o->p1), false);
			preg *tmp = alloc_register(ctx, RCPU);
			load_const(ctx, tmp, sizeof(void *),
			           (intptr_t)(m->globals_data + m->globals_indexes[o->p2]));
			emit_ldr(ctx, dst->holds->t->kind, 0, dst, tmp);
			break;
		}
		case OSetGlobal: {
			preg *tmp = alloc_register(ctx, RCPU);
			load_const(ctx, tmp, sizeof(void *),
			           (intptr_t)(m->globals_data + m->globals_indexes[o->p1]));
			preg *src = fetch(ctx, R(o->p2), true);
			emit_str(ctx, src->holds->t->kind, 0, false, false, src, tmp);
			break;
		}
		case OField: {
			switch (ra->t->kind) {
			case HOBJ:
			case HSTRUCT: {
				hl_runtime_obj *rt = hl_get_obj_rt(ra->t);
				preg *ra = fetch(ctx, R(o->p2), true);
				preg *dst = fetch(ctx, R(o->p1), false);
				emit_ldr(ctx, ra->holds->t->kind, rt->fields_indexes[o->p3],
				         dst, ra);
			} break;
			case HVIRTUAL:
				emit_brk(ctx, o->op);
				// ASM for --> if( hl_vfields(o)[f] ) r = *hl_vfields(o)[f];
				// else r = hl_dyn_get(o,hash(field),vt)
				break;
			default:
				ERROR("Expected HOBJ, HSTRUCT or HVIRTUAL.");
				break;
			}
			break;
		}
		case OSetField: {
			vreg *obj = R(o->p1);
			switch (obj->t->kind) {
			case HOBJ:
			case HSTRUCT: {
				hl_runtime_obj *rt = hl_get_obj_rt(obj->t);
				preg *ra = fetch(ctx, R(o->p2), true);
				preg *src = fetch(ctx, R(o->p1), true);
				emit_str(ctx, ra->holds->t->kind, rt->fields_indexes[o->p3],
				         false, false, src, ra);
			} break;
			case HVIRTUAL:
				emit_brk(ctx, o->op);
				// ASM for --> if( hl_vfields(o)[f] ) r = *hl_vfields(o)[f];
				// else r = hl_dyn_get(o,hash(field),vt)
				break;
			default:
				ERROR("Expected HOBJ, HSTRUCT or HVIRTUAL.");
				break;
			}
			break;
		}
		case OGetThis:
			emit_brk(ctx, o->op);
			break;
		case OSetThis:
			emit_brk(ctx, o->op);
			break;
		case ODynGet:
			emit_brk(ctx, o->op);
			break;
		case ODynSet:
			emit_brk(ctx, o->op);
			break;

		case OJTrue: {
			vreg *r = R(o->p1);
			emit_ari_imm(ctx, SUBS, false, 1, fetch(ctx, r, true), REG_AT(ZR));
			emit_cond_branch(ctx, EQ, 0);
			register_jump(ctx, BCOND, BUF_POS() - 4, opCount + 1 + o->p2);
		}; break;
		case OJFalse: {
			vreg *r = R(o->p1);
			emit_ari_imm(ctx, SUBS, false, 0, fetch(ctx, r, true), REG_AT(ZR));
			emit_cond_branch(ctx, EQ, 0);
			register_jump(ctx, BCOND, BUF_POS() - 4, opCount + 1 + o->p2);
		}; break;
		case OJNull: {
			vreg *r = R(o->p1);
			emit_ari_imm(ctx, SUBS, false, 0, fetch(ctx, r, true), REG_AT(ZR));
			emit_cond_branch(ctx, EQ, 0);
			register_jump(ctx, BCOND, BUF_POS() - 4, opCount + 1 + o->p2);
		}; break;
		case OJNotNull: {
			vreg *r = R(o->p1);
			emit_ari_imm(ctx, SUBS, false, 0, fetch(ctx, r, true), REG_AT(ZR));
			emit_cond_branch(ctx, NE, 0);
			register_jump(ctx, BCOND, BUF_POS() - 4, opCount + 1 + o->p2);
		}; break;
		case OJSLt:
		case OJSGte:
		case OJSGt:
		case OJSLte:
		case OJULt:
		case OJUGte:
		case OJNotLt:
		case OJNotGte:
		case OJEq:
		case OJNotEq: {
			vreg *a = R(o->p1);
			vreg *b = R(o->p2);
			CondCode cond;
			switch (o->op) {
			case OJSLt:
				cond = LT;
				break;
			case OJSGte:
				cond = GE;
				break;
			case OJSGt:
				cond = GT;
				break;
			case OJSLte:
				cond = LE;
				break;
			case OJULt:
				cond = CC;
				break;
			case OJUGte:
				cond = HI;
				break;
			case OJNotLt:
				cond = GE;
				break;
			case OJNotGte:
				cond = LT;
				break;
			case OJEq:
				cond = EQ;
				break;
			case OJNotEq:
				cond = NE;
				break;
			default:
				cond = EQ;
				break;
			}
			emit_ari_shift_reg(ctx, SUBS, false, fetch(ctx, a, true),
			                   fetch(ctx, b, true), REG_AT(ZR), LSL, 0);
			emit_cond_branch(ctx, cond, 0);
			register_jump(ctx, BCOND, BUF_POS() - 4, opCount + 1 + o->p3);
		}; break;
		case OJAlways:
			emit_uncond_branch_imm(ctx, B, 0);
			register_jump(ctx, B, BUF_POS() - 4, opCount + 1 + o->p1);
			break;

		case OToDyn:
			emit_brk(ctx, o->op);
			break;
		case OToSFloat:
			emit_brk(ctx, o->op);
			break;
		case OToUFloat:
			emit_brk(ctx, o->op);
			break;
		case OToInt:
			emit_brk(ctx, o->op);
			break;
		case OSafeCast:
			make_dyn_cast(ctx, R(o->p1), R(o->p2));
			break;
		case OUnsafeCast:
			emit_brk(ctx, o->op);
			break;
		case OToVirtual:
			emit_brk(ctx, o->op);
			break;

		case OLabel:
			break;
		case ORet:
			if (ctx->totalRegsSize) {
				emit_ari_imm(ctx, ADD, true, ctx->totalRegsSize, REG_AT(SP),
				             REG_AT(SP));
			}
			// ldp x29, x30, [sp], #16
			W(0xA8C17BFD);
			emit_uncond_branch_reg(ctx, RET, REG_AT(30));
			break;
		case OThrow:
			emit_brk(ctx, o->op);
			break;
		case ORethrow:
			emit_brk(ctx, o->op);
			break;
		case OSwitch:
			emit_brk(ctx, o->op);
			break;
		case ONullCheck: {
			vreg *r = R(o->p1);
			emit_ari_imm(ctx, SUBS, false, 0, fetch(ctx, r, true), REG_AT(ZR));
			emit_cond_branch(ctx, NE, 0);
			size_t pos = BUF_POS() - 4;
			start_call(ctx);
			load_const(ctx, REG_AT(17), sizeof(void *),
			           (int_val)hl_null_access);
			emit_uncond_branch_reg(ctx, BLR, REG_AT(17));
			end_call(ctx, 0);
			*(int*)(ctx->startBuf + pos) |= (((BUF_POS() - pos) & 0x7FFFF) << 5);
			break;
		}
		case OTrap:
			emit_brk(ctx, o->op);
			break;
		case OEndTrap:
			emit_brk(ctx, o->op);
			break;

		case OGetI8:
			emit_brk(ctx, o->op);
			break;
		case OGetI16:
			emit_brk(ctx, o->op);
			break;
		case OGetMem:
			emit_brk(ctx, o->op);
			break;
		case OGetArray:
			emit_brk(ctx, o->op);
			break;
		case OSetI8:
			emit_brk(ctx, o->op);
			break;
		case OSetI16:
			emit_brk(ctx, o->op);
			break;
		case OSetMem:
			emit_brk(ctx, o->op);
			break;
		case OSetArray:
			emit_brk(ctx, o->op);
			break;

		case ONew: {
			void *allocFun;
			switch (dst->t->kind) {
			case HOBJ:
			case HSTRUCT:
				allocFun = hl_alloc_obj;
				break;
			case HDYNOBJ:
				allocFun = hl_alloc_dynobj;
				nargs = 0;
				break;
			case HVIRTUAL:
				allocFun = hl_alloc_virtual;
				break;
			default:
				ERROR("Expect HOBJ, HSTRUCT or HVIRTUAL");
			}
			call_native_consts(ctx, dst, (intptr_t)allocFun, 1,
			                   (intptr_t[]){(intptr_t)dst->t});
			break;
		}
		case OArraySize:
			emit_brk(ctx, o->op);
			break;
		case OType:
			assert(o->p2 < m->code->ntypes);
			load_const(ctx, fetch(ctx, R(o->p1), false), sizeof(void *),
			           (intptr_t)(m->code->types + o->p2));
			break;
		case OGetType:
			emit_brk(ctx, o->op);
			break;
		case OGetTID:
			emit_brk(ctx, o->op);
			break;

		case ORef:
			emit_brk(ctx, o->op);
			break;
		case OUnref:
			emit_brk(ctx, o->op);
			break;
		case OSetref:
			emit_brk(ctx, o->op);
			break;

		case OMakeEnum:
			emit_brk(ctx, o->op);
			break;
		case OEnumAlloc:
			emit_brk(ctx, o->op);
			break;
		case OEnumIndex:
			emit_brk(ctx, o->op);
			break;
		case OEnumField:
			emit_brk(ctx, o->op);
			break;
		case OSetEnumField:
			emit_brk(ctx, o->op);
			break;

		case OAssert:
			emit_brk(ctx, o->op);
			break;
		case ORefData:
			emit_brk(ctx, o->op);
			break;
		case ORefOffset:
			emit_brk(ctx, o->op);
			break;
		case ONop:
			break;
		default:
			break;
		}
		ctx->opsPos[opCount + 1] = BUF_POS();

		// write debug infos
		int size = BUF_POS() - codePos;
		if (debug16 && size > 0xFF00) {
			debug32 = malloc(sizeof(int) * (f->nops + 1));
			for (int i = 0; i < ctx->currentPos; i++)
				debug32[i] = debug16[i];
			free(debug16);
			debug16 = NULL;
		}
		if (debug16)
			debug16[ctx->currentPos] = (unsigned short)size;
		else if (debug32)
			debug32[ctx->currentPos] = size;
	}
	{
		jlist *j = ctx->jumps;
		while (j) {
			int32_t offset;
			switch (j->op) {
			case B:
				offset =
				    (((ctx->opsPos[j->target] - (j->pos + 4)) / 4) & 0x3FFFFFF);
				break;
			case BCOND:
				offset =
				    (((ctx->opsPos[j->target] - (j->pos + 4)) / 4) & 0x7FFFF)
				    << 5;
				break;
			default:
				ERROR("Expected a branch.");
			}
			*(uint32_t *)(ctx->startBuf + j->pos) |= offset;
			j = j->next;
		}
		ctx->jumps = NULL;
	}
	// add nops padding
	jit_nops(ctx);
	// clear regs
	for (int i = 0; i < REG_COUNT; i++) {
		preg *r = REG_AT(i);
		r->holds = NULL;
		r->lock = 0;
	}
	// save debug infos
	{
		int fid = (int)(f - m->code->functions);
		ctx->debug[fid].start = codePos;
		ctx->debug[fid].offsets = debug32 ? (void *)debug32 : (void *)debug16;
		ctx->debug[fid].large = debug32 != NULL;
	}
	// reset tmp allocator
	hl_free(&ctx->falloc);
	return codePos;
}

jit_ctx *hl_jit_alloc() {
	jit_ctx *ctx = malloc(sizeof(jit_ctx));
	if (ctx == NULL)
		return NULL;
	memset(ctx, 0, sizeof(jit_ctx));

	hl_alloc_init(&ctx->falloc);
	hl_alloc_init(&ctx->galloc);
	for (int i = 0; i < RCPU_COUNT; i++) {
		preg *r = REG_AT(i);
		r->id = i;
		r->kind = RCPU;
	}
	for (int i = 0; i < RFPU_COUNT; i++) {
		preg *r = REG_AT(VREG(i));
		r->id = i;
		r->kind = RFPU;
	}
	preg *sp = REG_AT(SP);
	sp->id = 31;
	sp->kind = RCPU;
	return ctx;
}

void hl_jit_free(jit_ctx *ctx, h_bool can_reset) {
	free(ctx->vregs);
	free(ctx->opsPos);
	free(ctx->startBuf);
	ctx->maxRegs = 0;
	ctx->vregs = NULL;
	ctx->maxOps = 0;
	ctx->opsPos = NULL;
	ctx->startBuf = NULL;
	ctx->bufSize = 0;
	ctx->buf = NULL;
	ctx->calls = NULL;
	ctx->switchs = NULL;
	ctx->closure_list = NULL;
	hl_free(&ctx->falloc);
	hl_free(&ctx->galloc);
	if (!can_reset)
		free(ctx);
}

static void hl_jit_init_module(jit_ctx *ctx, hl_module *m) {
	ctx->m = m;
	if (m->code->hasdebug)
		ctx->debug = (hl_debug_infos *)malloc(sizeof(hl_debug_infos) *
		                                      m->code->nfunctions);
	// for (int i = 0;i < m->code->nfloats;i++) {
	//     jit_buf(ctx);
	//     *ctx->buf.d++ = m->code->floats[i];
	// }
}

void hl_jit_reset(jit_ctx *ctx, hl_module *m) {
	ctx->debug = NULL;
	hl_jit_init_module(ctx, m);
}

void hl_jit_init(jit_ctx *ctx, hl_module *m) { hl_jit_init_module(ctx, m); }

#ifdef __APPLE__
#include <libkern/OSCacheControl.h>
#endif

#if defined(__has_builtin)
#if __has_builtin(__builtin__clear_cache)
#define HAS_BUILTIN_CLEAR_CACHE
#endif
#endif

#ifndef HAS_BUILTIN_CLEAR_CACHE
#if defined(__clang__)
extern void __clear_cache(void *begin, void *end);
#elif defined(__GNUC__)
extern void __clear_cache(char *begin, char *end);
#endif
#endif

static inline void clear_cache(unsigned char *code, int size) {
#ifdef __APPLE__
	sys_icache_invalidate(code, size);
#elif defined(__clang__) || defined(__GNUC__)
#ifdef HAS_BUILTIN_CLEAR_CACHE
	__builtin___clear_cache((void *)code, (void *)(code + size));
#else
	__clear_cache((void *)code, (void *)(code + size));
#endif
#endif
}

void *hl_jit_code(jit_ctx *ctx, hl_module *m, int *codesize,
                  hl_debug_infos **debug, hl_module *previous) {
	size_t size = BUF_POS();
	unsigned char *code;
	if (size & 4095)
		size += 4096 - (size & 4095);
	code = (unsigned char *)hl_alloc_executable_memory(size);

	if (code == NULL)
		return NULL;
	*codesize = size;
	*debug = ctx->debug;
#ifdef __APPLE__
	pthread_jit_write_protect_np(false);
#endif
	memcpy(code, ctx->startBuf, BUF_POS());
	jlist *c = ctx->calls;
	while (c) {
		int64_t offset = 0;
		if (c->target < 0) {
			// fabs = ctx->static_functions[-c->target - 1];
		} else {
			intptr_t fabs = (intptr_t)m->functions_ptrs[c->target];
			if (fabs == 0) {
				// read absolute address from previous module
				int old_idx =
				    m->hash->functions_hashes[m->functions_indexes[c->target]];
				if (old_idx < 0)
					return NULL;
				intptr_t abs_pos =
				    (intptr_t)previous->functions_ptrs
				        [(previous->code->functions + old_idx)->findex];
				offset = (int64_t)abs_pos - (int64_t)&code[c->pos];
			} else {
				// relative
				offset = ((intptr_t)fabs > (intptr_t)c->pos
				              ? (intptr_t)fabs - (intptr_t)c->pos
				              : -(int64_t)((intptr_t)c->pos - (intptr_t)fabs)) /
				         4;
				assert((&code[c->pos] + (offset * 4)) < (code + size));
			}
		}
		if (offset >= (128 * (1 << 20)) || offset <= -(128 * (1 << 20))) {
			TODO(
			    "Function calls with a pc relative offset of more that +/- 128 "
			    "MB\noffset %.5f MB",
			    (double)offset / (double)(1 << 20));
		}
		code[c->pos] |= (offset & 0x03FFFFFF);
		c = c->next;
	}
#ifdef __APPLE__
	pthread_jit_write_protect_np(true);
#endif
	// invalidate the instruction cache
	clear_cache(code, size);
	*codesize = size;
	*debug = ctx->debug;
	return code;
}

void hl_jit_patch_method(void *old_fun, void **new_fun_table) {}