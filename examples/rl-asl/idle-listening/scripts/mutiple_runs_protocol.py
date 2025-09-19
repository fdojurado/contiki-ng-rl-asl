# This script runs multiple times cooja to obtain data from the simulation

import argparse
import os
import random
import logging
from rich.logging import RichHandler


def main(contiki_path: str, simulation_file: str, output_folder: str, num_runs: int) -> None:
    assert isinstance(contiki_path, str), "Contiki path must be a string"
    assert os.path.exists(contiki_path), "Contiki path does not exist"
    assert isinstance(simulation_file, str), "Simulation file must be a string"
    assert os.path.exists(simulation_file), "Simulation file does not exist"
    assert isinstance(output_folder, str), "Output folder must be a string"
    assert os.path.exists(output_folder), "Output folder does not exist"
    assert isinstance(num_runs, int) and num_runs > 0, "Number of runs must be a positive integer"

    # -------------------- Create logger --------------------
    # cooja path
    cooja_path = f'{contiki_path}/tools/cooja'
    # Get absolute path
    output_folder = os.path.abspath(output_folder)
    # Create the command to run the simulation
    command = f"cd {cooja_path} && ./gradlew run --args='--no-gui {simulation_file} --contiki={contiki_path}"
    # Print/log info all the information
    # logger.info(f'Contiki path: {contiki_path}')
    # logger.info(f'Cooja path: {cooja_path}')
    # logger.info(f'Simulation file: {simulation_file}')
    # logger.info(f'Output folder: {output_folder}')
    # # logger.info(f'File name: {file_name}')
    # logger.info(f'Command: {command}')
    # We run the simulation n times
    for i in range(num_runs):
        # Delete any COOJA log file
        os.system(f'rm -rf {output_folder}/COOJA.*')
        # Generate the random seed for the simulation set of 5 integers
        random_seed = random.randint(0, 100000)
        # Add the random seed to the command
        exec_command = f"{command} --random-seed={random_seed}' --stacktrace"
        # Lets first remove everything in the build folder
        root_folder_simulation = os.path.dirname(simulation_file)
        clean_command = f'rm -rf {root_folder_simulation}/build'
        os.system(clean_command)
        # We run the simulation using the command line
        print(
            f'Running simulation {i+1}/{num_runs} command: {exec_command}')
        os.system(clean_command)
        os.system(exec_command)
        # list where am I
        os.system('pwd')
        # We only move the .testlog file if there is a TEST OK in the file
        test_ok = 0
        with open(f'{cooja_path}/COOJA.testlog', 'r') as file:
            for line in file:
                if 'TEST OK' in line:
                    test_ok = 1
                    break
        # # We move the .rtf files to the output folder
        if test_ok == 1:
            os.system(
                f'mv {cooja_path}/COOJA.testlog {output_folder}/{i}.testlog')
        # os.system(f'mv *.testlog {output_folder}/{file_name}_{i}.testlog')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-n', '--num_runs', type=int, default=10,
                        help='Number of runs to execute the simulation')
    # simulation file
    parser.add_argument('-s', '--simulation', type=str, default='/Users/fabian/contiki-ng-sage/examples/sage-project/sage/topology-a.csc',
                        help='Path to the simulation file')
    # Contiki-ng path
    parser.add_argument('-c', '--contiki', type=str, default='/Users/fabian/contiki-ng-sage',
                        help='Path to the contiki-ng folder')
    parser.add_argument('-o', '--output', type=str, default='/Users/fabian/contiki-ng-sage/examples/sage-project/sage/data/samples',
                        help='Output folder to store the samples')
    # Log directory
    parser.add_argument('-l', '--logdir', type=str, default='/Users/fabian/contiki-ng-sage/examples/sage-project/sage/data/samples',
                        help='Log directory to store the logs')
    # name of the output file
    # parser.add_argument('-f', '--file', type=str, required=True,
    #                     help='Name of the output file')
    args = parser.parse_args()
    # If the output folder does not exist, we create it
    if not os.path.exists(args.output):
        os.makedirs(args.output)
    main(contiki_path=args.contiki,
         simulation_file=args.simulation,
         output_folder=args.output,
         num_runs=args.num_runs)


# python3 scripts/mutiple_runs_protocol.py -n 5 -s ~/contiki-ng-sage/examples/sage-project/sage/topology-a.csc -o ~/contiki-ng-sage/examples/sage-project/sage/data/samples/0.8_0.1_0.1/