#!/usr/bin/env python3
"""Supply hardware FPATAN where Unicorn otherwise narrows to binary64.

The original surrounding Watcom code still executes. Each replaced instruction
is an explicit capture dependency; x87 register words/tags and TOP are updated
without converting through a Python float. This adapter is verification-only.
"""
import ctypes
import os
from pathlib import Path
import struct
from native_reference import r



def reference_library(build):
    """The original-executable oracle belongs to the faithful tfx-cpp build."""
    configured = os.environ.get('TFX_NATIVE_X87_LIBRARY')
    if configured:
        return ctypes.CDLL(str(Path(configured).resolve()))
    # Accept an existing capture setup as well as the usual sibling checkout.
    candidates = (Path(build) / 'libnative_x87_backend.so',
                  Path(__file__).resolve().parents[2] / 'tfx-cpp/build/libnative_x87_backend.so')
    for path in candidates:
        if path.is_file():
            return ctypes.CDLL(str(path.resolve()))
    raise RuntimeError(
        "Original-executable capture needs tfx-cpp's native_x87_backend. "
        'Build that target in ../tfx-cpp/build, or set TFX_NATIVE_X87_LIBRARY '
        'to its shared library. Redux itself and its ordinary tests do not need it.')


def hardware_fpatan(image,build):
    library=reference_library(build)
    operation=library.tfx_fpatan
    operation.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_ushort]
    operation.restype=None
    def replace(im):
        status=im.cpu.reg_read(r.UC_X86_REG_FPSW);top=(status>>11)&7;destination=(top+1)&7
        x=im.cpu.reg_read(getattr(r,'UC_X86_REG_FP'+str(top)))
        y=im.cpu.reg_read(getattr(r,'UC_X86_REG_FP'+str(destination)))
        xb=ctypes.create_string_buffer(struct.pack('<QH',*x));yb=ctypes.create_string_buffer(struct.pack('<QH',*y));out=ctypes.create_string_buffer(10)
        operation(yb,xb,out,im.cpu.reg_read(r.UC_X86_REG_FPCW))
        value=struct.unpack('<QH',out.raw)
        im.cpu.reg_write(getattr(r,'UC_X86_REG_FP'+str(destination)),value)
        tags=im.cpu.reg_read(r.UC_X86_REG_FPTAG)
        tag=1 if value[0]==0 and value[1]&0x7fff==0 else 0
        tags=(tags|(3<<(top*2)))&~(3<<(destination*2));tags|=tag<<(destination*2)
        im.cpu.reg_write(r.UC_X86_REG_FPTAG,tags)
        im.cpu.reg_write(r.UC_X86_REG_FPSW,(status&~0x3a00)|(destination<<11))
        im.cpu.reg_write(r.UC_X86_REG_EIP,im.cpu.reg_read(r.UC_X86_REG_EIP)+2)
    for file in (0xd2469,0xd248b,0xd2498,0xd24b2):
        image.intercept(file,'Use actual 80-bit host FPATAN; installed Unicorn narrows both operands and result to binary64',replace)
    return library


def hardware_sine_cosine(image,build):
    library=reference_library(build)
    operation=library.tfx_sine_cosine
    operation.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_ushort,ctypes.c_uint]
    operation.restype=None
    def replace(im,cosine):
        status=im.cpu.reg_read(r.UC_X86_REG_FPSW);top=(status>>11)&7
        register=getattr(r,'UC_X86_REG_FP'+str(top));value=im.cpu.reg_read(register)
        incoming=ctypes.create_string_buffer(struct.pack('<QH',*value));out=ctypes.create_string_buffer(10)
        operation(incoming,out,im.cpu.reg_read(r.UC_X86_REG_FPCW),cosine)
        result=struct.unpack('<QH',out.raw);im.cpu.reg_write(register,result)
        tag=1 if result[0]==0 and result[1]&0x7fff==0 else 0
        tags=im.cpu.reg_read(r.UC_X86_REG_FPTAG)&~(3<<(top*2));tags|=tag<<(top*2)
        im.cpu.reg_write(r.UC_X86_REG_FPTAG,tags)
        im.cpu.reg_write(r.UC_X86_REG_FPSW,status&~0x600)
        im.cpu.reg_write(r.UC_X86_REG_EIP,im.cpu.reg_read(r.UC_X86_REG_EIP)+2)
    image.intercept(0xd22f2,'Use actual 80-bit FSIN for bounded angles; Unicorn narrows to binary64',lambda im:replace(im,0))
    image.intercept(0xd22dd,'Use actual 80-bit FCOS for bounded angles; Unicorn narrows to binary64',lambda im:replace(im,1))
    return library


def hardware_tangent(image,build):
    library=reference_library(build)
    operation=library.tfx_tangent
    operation.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_ushort]
    operation.restype=None
    def replace(im):
        status=im.cpu.reg_read(r.UC_X86_REG_FPSW);top=(status>>11)&7;newtop=(top-1)&7
        register=getattr(r,'UC_X86_REG_FP'+str(top));value=im.cpu.reg_read(register)
        incoming=ctypes.create_string_buffer(struct.pack('<QH',*value));out=ctypes.create_string_buffer(10)
        operation(incoming,out,im.cpu.reg_read(r.UC_X86_REG_FPCW))
        result=struct.unpack('<QH',out.raw);im.cpu.reg_write(register,result)
        im.cpu.reg_write(getattr(r,'UC_X86_REG_FP'+str(newtop)),(1<<63,0x3fff))
        tag=1 if result[0]==0 and result[1]&0x7fff==0 else 0
        tags=im.cpu.reg_read(r.UC_X86_REG_FPTAG)&~((3<<(top*2))|(3<<(newtop*2)));tags|=tag<<(top*2)
        im.cpu.reg_write(r.UC_X86_REG_FPTAG,tags)
        im.cpu.reg_write(r.UC_X86_REG_FPSW,(status&~0x3e00)|(newtop<<11))
        im.cpu.reg_write(r.UC_X86_REG_EIP,im.cpu.reg_read(r.UC_X86_REG_EIP)+2)
    image.intercept(0xd23be,'Use actual 80-bit FPTAN for bounded angles; Unicorn narrows to binary64',replace)
    return library
