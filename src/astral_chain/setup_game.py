"""Compat entry point. The setup CLI moved to astral_chain.build.setup_game.

  python -m astral_chain.setup_game [options]
  (canonical: python -m astral_chain.build.setup_game [options])
"""
from astral_chain.build.setup_game import main

if __name__ == '__main__':
    main()
