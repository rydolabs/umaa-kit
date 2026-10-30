"""Local orchestration, schema creation and offline worker.

Production schema changes use migrations; `init-dev` is not a migration manager.
"""
import argparse
import os
from pathlib import Path
from .platform_db import connect, Base
from .platform_worker import run_one


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=['init-dev','work-once','work-until-empty'])
    args = parser.parse_args(argv)
    url = os.environ['STUDIO_DATABASE_URL']
    storage = Path(os.environ['STUDIO_STORE_DIR'])
    engine, sessions = connect(url)
    if args.action == 'init-dev':
        Base.metadata.create_all(engine)
        print('Development schema created; production requires migrations')
        return 0
    count = 0
    while True:
        result = run_one(sessions, storage)
        if result is None:
            break
        count += 1
        print('Completed job', result)
        if args.action == 'work-once':
            break
    print('Processed', count, 'job(s)')
    return 0
if __name__=='__main__':
    raise SystemExit(main())
