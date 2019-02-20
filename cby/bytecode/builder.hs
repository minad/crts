-- Generated by generate.pl from insn.defs and priminsn.defs

firstPrim :: Int
firstPrim = 87

insn_idx1 :: (Builder m, Imm i0) => Reg -> Reg -> i0 -> m ()
insn_idx1 res var i0 = do
  u16 (0 :: Word16)
  u8 (unReg res)
  u8 (unReg var)
  u8 i0

insn_idx2 :: (Builder m, Imm i0, Imm i1) => Reg -> Reg -> i0 -> i1 -> m ()
insn_idx2 res var i0 i1 = do
  u16 (1 :: Word16)
  u8 (unReg res)
  u8 (unReg var)
  u8 i0
  u8 i1

insn_idxl1 :: (Builder m, Imm i0) => Reg -> Reg -> i0 -> m ()
insn_idxl1 res var i0 = do
  u16 (2 :: Word16)
  u16 (unReg res)
  u16 (unReg var)
  u16 i0

insn_idxl2 :: (Builder m, Imm i0, Imm i1) => Reg -> Reg -> i0 -> i1 -> m ()
insn_idxl2 res var i0 i1 = do
  u16 (3 :: Word16)
  u16 (unReg res)
  u16 (unReg var)
  u16 i0
  u16 i1

insn_fset :: (Builder m, Imm i) => Reg -> i -> Reg -> m ()
insn_fset var i field = do
  u16 (4 :: Word16)
  u16 (unReg var)
  u16 i
  u16 (unReg field)

insn_tset :: (Builder m, Imm i) => Reg -> i -> Reg -> m ()
insn_tset var i field = do
  u16 (5 :: Word16)
  u16 (unReg var)
  u16 i
  u16 (unReg field)

insn_int8 :: (Builder m, Integral val) => Reg -> val -> m ()
insn_int8 res val = do
  u16 (6 :: Word16)
  u8 (unReg res)
  i8 val

insn_int32 :: (Builder m, Integral val) => Reg -> val -> m ()
insn_int32 res val = do
  u16 (7 :: Word16)
  u16 (unReg res)
  i32 val

insn_int64 :: (Builder m, Integral val) => Reg -> val -> m ()
insn_int64 res val = do
  u16 (8 :: Word16)
  u16 (unReg res)
  i64 val

insn_uint8 :: (Builder m, Imm val) => Reg -> val -> m ()
insn_uint8 res val = do
  u16 (9 :: Word16)
  u8 (unReg res)
  u8 val

insn_uint32 :: (Builder m, Imm val) => Reg -> val -> m ()
insn_uint32 res val = do
  u16 (10 :: Word16)
  u16 (unReg res)
  u32 val

insn_uint64 :: (Builder m, Imm val) => Reg -> val -> m ()
insn_uint64 res val = do
  u16 (11 :: Word16)
  u16 (unReg res)
  u64 val

insn_float32 :: (Builder m) => Reg -> Float -> m ()
insn_float32 res val = do
  u16 (12 :: Word16)
  u16 (unReg res)
  u32 (castFloatToWord32 val)

insn_float64 :: (Builder m) => Reg -> Double -> m ()
insn_float64 res val = do
  u16 (13 :: Word16)
  u16 (unReg res)
  u64 (castDoubleToWord64 val)

insn_string :: (Builder m, ConvertString bref ByteString) => Reg -> bref -> m ()
insn_string res bref = do
  u16 (14 :: Word16)
  u16 (unReg res)
  bytesref bref

insn_buffer :: (Builder m, ConvertString bref ByteString) => Reg -> bref -> m ()
insn_buffer res bref = do
  u16 (15 :: Word16)
  u16 (unReg res)
  bytesref bref

insn_xint :: (Builder m, ConvertString bref ByteString) => Reg -> bref -> m ()
insn_xint res bref = do
  u16 (16 :: Word16)
  u16 (unReg res)
  bytesref bref

insn_xintn :: (Builder m, ConvertString bref ByteString) => Reg -> bref -> m ()
insn_xintn res bref = do
  u16 (17 :: Word16)
  u16 (unReg res)
  bytesref bref

insn_xint8 :: (Builder m, Imm val) => Reg -> val -> m ()
insn_xint8 res val = do
  u16 (18 :: Word16)
  u8 (unReg res)
  u8 val

insn_xint64 :: (Builder m, Imm val) => Reg -> val -> m ()
insn_xint64 res val = do
  u16 (19 :: Word16)
  u16 (unReg res)
  u64 val

insn_movc2 :: (Builder m) => Reg -> Reg -> m ()
insn_movc2 res src = do
  u16 (20 :: Word16)
  u8 (unReg res)
  u8 (unReg src)

insn_movc3 :: (Builder m) => Reg -> Reg -> m ()
insn_movc3 res src = do
  u16 (21 :: Word16)
  u8 (unReg res)
  u8 (unReg src)

insn_movc4 :: (Builder m) => Reg -> Reg -> m ()
insn_movc4 res src = do
  u16 (22 :: Word16)
  u8 (unReg res)
  u8 (unReg src)

insn_movcl2 :: (Builder m) => Reg -> Reg -> m ()
insn_movcl2 res src = do
  u16 (23 :: Word16)
  u16 (unReg res)
  u16 (unReg src)

insn_movcl3 :: (Builder m) => Reg -> Reg -> m ()
insn_movcl3 res src = do
  u16 (24 :: Word16)
  u16 (unReg res)
  u16 (unReg src)

insn_movcl4 :: (Builder m) => Reg -> Reg -> m ()
insn_movcl4 res src = do
  u16 (25 :: Word16)
  u16 (unReg res)
  u16 (unReg src)

insn_mov1 :: (Builder m) => Reg -> Reg -> m ()
insn_mov1 res src0 = do
  u16 (26 :: Word16)
  u8 (unReg res)
  u8 (unReg src0)

insn_mov2 :: (Builder m) => Reg -> Reg -> Reg -> m ()
insn_mov2 res src0 src1 = do
  u16 (27 :: Word16)
  u8 (unReg res)
  u8 (unReg src0)
  u8 (unReg src1)

insn_mov3 :: (Builder m) => Reg -> Reg -> Reg -> Reg -> m ()
insn_mov3 res src0 src1 src2 = do
  u16 (28 :: Word16)
  u8 (unReg res)
  u8 (unReg src0)
  u8 (unReg src1)
  u8 (unReg src2)

insn_mov4 :: (Builder m) => Reg -> Reg -> Reg -> Reg -> Reg -> m ()
insn_mov4 res src0 src1 src2 src3 = do
  u16 (29 :: Word16)
  u8 (unReg res)
  u8 (unReg src0)
  u8 (unReg src1)
  u8 (unReg src2)
  u8 (unReg src3)

insn_movl1 :: (Builder m) => Reg -> Reg -> m ()
insn_movl1 res src0 = do
  u16 (30 :: Word16)
  u16 (unReg res)
  u16 (unReg src0)

insn_movl2 :: (Builder m) => Reg -> Reg -> Reg -> m ()
insn_movl2 res src0 src1 = do
  u16 (31 :: Word16)
  u16 (unReg res)
  u16 (unReg src0)
  u16 (unReg src1)

insn_thk :: (Builder m, Imm nclos) => Reg -> TopName -> nclos -> m ()
insn_thk res fnref nclos = do
  u16 (32 :: Word16)
  u16 (unReg res)
  ref (FnRef fnref)
  u16 nclos

insn_thk0 :: (Builder m) => Reg -> TopName -> m ()
insn_thk0 res fnref = do
  u16 (33 :: Word16)
  u8 (unReg res)
  ref (FnRef fnref)

insn_thk1 :: (Builder m) => Reg -> Reg -> TopName -> m ()
insn_thk1 res c0 fnref = do
  u16 (34 :: Word16)
  u8 (unReg res)
  u8 (unReg c0)
  ref (FnRef fnref)

insn_thk2 :: (Builder m) => Reg -> Reg -> Reg -> TopName -> m ()
insn_thk2 res c0 c1 fnref = do
  u16 (35 :: Word16)
  u8 (unReg res)
  u8 (unReg c0)
  u8 (unReg c1)
  ref (FnRef fnref)

insn_thk3 :: (Builder m) => Reg -> Reg -> Reg -> Reg -> TopName -> m ()
insn_thk3 res c0 c1 c2 fnref = do
  u16 (36 :: Word16)
  u8 (unReg res)
  u8 (unReg c0)
  u8 (unReg c1)
  u8 (unReg c2)
  ref (FnRef fnref)

insn_thk4 :: (Builder m) => Reg -> Reg -> Reg -> Reg -> Reg -> TopName -> m ()
insn_thk4 res c0 c1 c2 c3 fnref = do
  u16 (37 :: Word16)
  u8 (unReg res)
  u8 (unReg c0)
  u8 (unReg c1)
  u8 (unReg c2)
  u8 (unReg c3)
  ref (FnRef fnref)

insn_fn :: (Builder m, Imm arity, Imm nclos) => Reg -> TopName -> arity -> nclos -> m ()
insn_fn res fnref arity nclos = do
  u16 (38 :: Word16)
  u16 (unReg res)
  ref (FnRef fnref)
  u8 arity
  u16 nclos

insn_fn0 :: (Builder m, Imm arity) => Reg -> arity -> TopName -> m ()
insn_fn0 res arity fnref = do
  u16 (39 :: Word16)
  u8 (unReg res)
  u8 arity
  ref (FnRef fnref)

insn_fn1 :: (Builder m, Imm arity) => Reg -> arity -> Reg -> TopName -> m ()
insn_fn1 res arity c0 fnref = do
  u16 (40 :: Word16)
  u8 (unReg res)
  u8 arity
  u8 (unReg c0)
  ref (FnRef fnref)

insn_fn2 :: (Builder m, Imm arity) => Reg -> arity -> Reg -> Reg -> TopName -> m ()
insn_fn2 res arity c0 c1 fnref = do
  u16 (41 :: Word16)
  u8 (unReg res)
  u8 arity
  u8 (unReg c0)
  u8 (unReg c1)
  ref (FnRef fnref)

insn_fn3 :: (Builder m, Imm arity) => Reg -> arity -> Reg -> Reg -> Reg -> TopName -> m ()
insn_fn3 res arity c0 c1 c2 fnref = do
  u16 (42 :: Word16)
  u8 (unReg res)
  u8 arity
  u8 (unReg c0)
  u8 (unReg c1)
  u8 (unReg c2)
  ref (FnRef fnref)

insn_fn4 :: (Builder m, Imm arity) => Reg -> arity -> Reg -> Reg -> Reg -> Reg -> TopName -> m ()
insn_fn4 res arity c0 c1 c2 c3 fnref = do
  u16 (43 :: Word16)
  u8 (unReg res)
  u8 arity
  u8 (unReg c0)
  u8 (unReg c1)
  u8 (unReg c2)
  u8 (unReg c3)
  ref (FnRef fnref)

insn_data :: (Builder m, Imm tag, Imm nargs) => Reg -> tag -> nargs -> m ()
insn_data res tag nargs = do
  u16 (44 :: Word16)
  u16 (unReg res)
  u16 tag
  u16 nargs

insn_data0 :: (Builder m, Imm tag) => Reg -> tag -> m ()
insn_data0 res tag = do
  u16 (45 :: Word16)
  u8 (unReg res)
  u8 tag

insn_datal0 :: (Builder m, Imm tag) => Reg -> tag -> m ()
insn_datal0 res tag = do
  u16 (46 :: Word16)
  u16 (unReg res)
  u16 tag

insn_data1 :: (Builder m, Imm tag) => Reg -> tag -> Reg -> m ()
insn_data1 res tag a0 = do
  u16 (47 :: Word16)
  u8 (unReg res)
  u8 tag
  u8 (unReg a0)

insn_data2 :: (Builder m, Imm tag) => Reg -> tag -> Reg -> Reg -> m ()
insn_data2 res tag a0 a1 = do
  u16 (48 :: Word16)
  u8 (unReg res)
  u8 tag
  u8 (unReg a0)
  u8 (unReg a1)

insn_data3 :: (Builder m, Imm tag) => Reg -> tag -> Reg -> Reg -> Reg -> m ()
insn_data3 res tag a0 a1 a2 = do
  u16 (49 :: Word16)
  u8 (unReg res)
  u8 tag
  u8 (unReg a0)
  u8 (unReg a1)
  u8 (unReg a2)

insn_data4 :: (Builder m, Imm tag) => Reg -> tag -> Reg -> Reg -> Reg -> Reg -> m ()
insn_data4 res tag a0 a1 a2 a3 = do
  u16 (50 :: Word16)
  u8 (unReg res)
  u8 tag
  u8 (unReg a0)
  u8 (unReg a1)
  u8 (unReg a2)
  u8 (unReg a3)

insn_casetbl :: (Builder m, Imm lo, Imm hi) => Reg -> lo -> hi -> m ()
insn_casetbl scrut lo hi = do
  u16 (51 :: Word16)
  u16 (unReg scrut)
  u32 lo
  u32 hi

insn_case :: (Builder m, Imm nalts) => Reg -> nalts -> m ()
insn_case scrut nalts = do
  u16 (52 :: Word16)
  u16 (unReg scrut)
  u16 nalts

insn_cont :: (Builder m, Imm off) => Reg -> off -> m ()
insn_cont top off = do
  u16 (53 :: Word16)
  u8 (unReg top)
  u8 off

insn_contl :: (Builder m, Imm off) => Reg -> off -> m ()
insn_contl top off = do
  u16 (54 :: Word16)
  u16 (unReg top)
  u32 off

insn_ret :: (Builder m) => Reg -> m ()
insn_ret var = do
  u16 (55 :: Word16)
  u8 (unReg var)

insn_retl :: (Builder m) => Reg -> m ()
insn_retl var = do
  u16 (56 :: Word16)
  u16 (unReg var)

insn_retif :: (Builder m) => Reg -> Reg -> m ()
insn_retif scrut var = do
  u16 (57 :: Word16)
  u8 (unReg scrut)
  u8 (unReg var)

insn_retifn :: (Builder m) => Reg -> Reg -> m ()
insn_retifn scrut var = do
  u16 (58 :: Word16)
  u8 (unReg scrut)
  u8 (unReg var)

insn_if :: (Builder m, Imm off) => Reg -> off -> m ()
insn_if scrut off = do
  u16 (59 :: Word16)
  u8 (unReg scrut)
  u8 off

insn_ifl :: (Builder m, Imm off) => Reg -> off -> m ()
insn_ifl scrut off = do
  u16 (60 :: Word16)
  u16 (unReg scrut)
  u32 off

insn_jmp :: (Builder m) => Reg -> TopName -> m ()
insn_jmp fn fnref = do
  u16 (61 :: Word16)
  u16 (unReg fn)
  ref (FnRef fnref)

insn_jmp1 :: (Builder m) => Reg -> Reg -> TopName -> m ()
insn_jmp1 fn a0 fnref = do
  u16 (62 :: Word16)
  u8 (unReg fn)
  u8 (unReg a0)
  ref (FnRef fnref)

insn_jmp2 :: (Builder m) => Reg -> Reg -> Reg -> TopName -> m ()
insn_jmp2 fn a0 a1 fnref = do
  u16 (63 :: Word16)
  u8 (unReg fn)
  u8 (unReg a0)
  u8 (unReg a1)
  ref (FnRef fnref)

insn_jmp3 :: (Builder m) => Reg -> Reg -> Reg -> Reg -> TopName -> m ()
insn_jmp3 fn a0 a1 a2 fnref = do
  u16 (64 :: Word16)
  u8 (unReg fn)
  u8 (unReg a0)
  u8 (unReg a1)
  u8 (unReg a2)
  ref (FnRef fnref)

insn_jmp4 :: (Builder m) => Reg -> Reg -> Reg -> Reg -> Reg -> TopName -> m ()
insn_jmp4 fn a0 a1 a2 a3 fnref = do
  u16 (65 :: Word16)
  u8 (unReg fn)
  u8 (unReg a0)
  u8 (unReg a1)
  u8 (unReg a2)
  u8 (unReg a3)
  ref (FnRef fnref)

insn_jmp5 :: (Builder m) => Reg -> Reg -> Reg -> Reg -> Reg -> Reg -> TopName -> m ()
insn_jmp5 fn a0 a1 a2 a3 a4 fnref = do
  u16 (66 :: Word16)
  u8 (unReg fn)
  u8 (unReg a0)
  u8 (unReg a1)
  u8 (unReg a2)
  u8 (unReg a3)
  u8 (unReg a4)
  ref (FnRef fnref)

insn_app :: (Builder m, Imm nargs) => Reg -> nargs -> m ()
insn_app fn nargs = do
  u16 (67 :: Word16)
  u8 (unReg fn)
  u8 nargs

insn_appn :: (Builder m, Imm nargs) => Reg -> nargs -> m ()
insn_appn fn nargs = do
  u16 (68 :: Word16)
  u16 (unReg fn)
  u8 nargs

insn_app1 :: (Builder m) => Reg -> Reg -> m ()
insn_app1 fn a0 = do
  u16 (69 :: Word16)
  u8 (unReg fn)
  u8 (unReg a0)

insn_app2 :: (Builder m) => Reg -> Reg -> Reg -> m ()
insn_app2 fn a0 a1 = do
  u16 (70 :: Word16)
  u8 (unReg fn)
  u8 (unReg a0)
  u8 (unReg a1)

insn_app3 :: (Builder m) => Reg -> Reg -> Reg -> Reg -> m ()
insn_app3 fn a0 a1 a2 = do
  u16 (71 :: Word16)
  u8 (unReg fn)
  u8 (unReg a0)
  u8 (unReg a1)
  u8 (unReg a2)

insn_app4 :: (Builder m) => Reg -> Reg -> Reg -> Reg -> Reg -> m ()
insn_app4 fn a0 a1 a2 a3 = do
  u16 (72 :: Word16)
  u8 (unReg fn)
  u8 (unReg a0)
  u8 (unReg a1)
  u8 (unReg a2)
  u8 (unReg a3)

insn_app5 :: (Builder m) => Reg -> Reg -> Reg -> Reg -> Reg -> Reg -> m ()
insn_app5 fn a0 a1 a2 a3 a4 = do
  u16 (73 :: Word16)
  u8 (unReg fn)
  u8 (unReg a0)
  u8 (unReg a1)
  u8 (unReg a2)
  u8 (unReg a3)
  u8 (unReg a4)

insn_clos :: (Builder m, Imm nargs, Imm limit, Imm size) => nargs -> limit -> size -> m ()
insn_clos nargs limit size = do
  u16 (74 :: Word16)
  u8 nargs
  u16 limit
  u16 size

insn_clos0 :: (Builder m, Imm nargs, Imm lim) => nargs -> lim -> m ()
insn_clos0 nargs lim = do
  u16 (75 :: Word16)
  u8 nargs
  u8 lim

insn_clos1 :: (Builder m, Imm nargs, Imm lim) => nargs -> lim -> m ()
insn_clos1 nargs lim = do
  u16 (76 :: Word16)
  u8 nargs
  u8 lim

insn_clos2 :: (Builder m, Imm nargs, Imm lim) => nargs -> lim -> m ()
insn_clos2 nargs lim = do
  u16 (77 :: Word16)
  u8 nargs
  u8 lim

insn_clos3 :: (Builder m, Imm nargs, Imm lim) => nargs -> lim -> m ()
insn_clos3 nargs lim = do
  u16 (78 :: Word16)
  u8 nargs
  u8 lim

insn_clos4 :: (Builder m, Imm nargs, Imm lim) => nargs -> lim -> m ()
insn_clos4 nargs lim = do
  u16 (79 :: Word16)
  u8 nargs
  u8 lim

insn_enter :: (Builder m, Imm top, Imm lim) => top -> lim -> m ()
insn_enter top lim = do
  u16 (80 :: Word16)
  u8 top
  u8 lim

insn_enterl :: (Builder m, Imm top, Imm lim) => top -> lim -> m ()
insn_enterl top lim = do
  u16 (81 :: Word16)
  u16 top
  u16 lim

insn_ffiget :: (Builder m) => Reg -> FFI -> m ()
insn_ffiget res ffi = do
  u16 (82 :: Word16)
  u16 (unReg res)
  ref (FFIRef (ffiNameWithoutHeader ffi))

insn_ffiset :: (Builder m) => Reg -> FFI -> m ()
insn_ffiset var ffi = do
  u16 (83 :: Word16)
  u16 (unReg var)
  ref (FFIRef (ffiNameWithoutHeader ffi))

insn_ffitail :: (Builder m, Imm nargs) => FFI -> nargs -> m ()
insn_ffitail ffi nargs = do
  u16 (84 :: Word16)
  ref (FFIRef (ffiNameWithoutHeader ffi))
  u8 nargs

insn_ffiinl :: (Builder m, Imm nargs) => Reg -> FFI -> nargs -> m ()
insn_ffiinl res ffi nargs = do
  u16 (85 :: Word16)
  u16 (unReg res)
  ref (FFIRef (ffiNameWithoutHeader ffi))
  u8 nargs

insn_ffiprot :: (Builder m, Imm nargs) => FFI -> nargs -> m ()
insn_ffiprot ffi nargs = do
  u16 (86 :: Word16)
  ref (FFIRef (ffiNameWithoutHeader ffi))
  u8 nargs
