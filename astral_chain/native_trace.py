"""Compat entry point. The translator CLI moved to astral_chain.translator.native_trace.

  python -m astral_chain.native_trace [options]
  (canonical: python -m astral_chain.translator.native_trace [options])
"""
from astral_chain.translator.native_trace import main

if __name__ == '__main__':
    main()
