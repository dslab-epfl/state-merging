from __future__ import with_statement

from analysis import profiling
import tempfile
import sys

def main():
    exppath = sys.argv[1] if len(sys.argv) > 1 else "."
    _, eventEntries = profiling.parse_klee_dir(exppath)

    with tempfile.NamedTemporaryFile(delete=False) as f:
        profiling.gen_multiplicity_distribution(f, eventEntries)
        print f.name
        
if __name__ == "__main__":
    main()
