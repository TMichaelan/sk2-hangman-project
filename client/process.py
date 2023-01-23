import concurrent.futures
import subprocess

script = 'main.py'

with concurrent.futures.ThreadPoolExecutor() as executor:
    tasks = [executor.submit(subprocess.run, ['python', script]) for _ in range(15)]
    concurrent.futures.wait(tasks)