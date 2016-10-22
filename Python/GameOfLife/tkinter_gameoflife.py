from tkinter import ttk, Tk, StringVar, PhotoImage, TclError
import tkinter as tk
import random
import functools
import sys


class Cell:
    CELL_TEXT_SIZE = 1
    NOBODY = "."
    CREATURE = "@"

    def __init__(self, game, row, col, images={}, cell_text_size=CELL_TEXT_SIZE):
        self.game = game
        self.mainframe = game.mainframe
        self.images = images
        self.cell_text_size = cell_text_size

        self.cell_variable = StringVar()
        self.cell_variable.set(self.NOBODY)
        self.make_button(self.invert)
        self.button.grid(row=row, column=col)

    def set(self, value):
        self.cell_variable.set(value)

    def get(self):
        return self.cell_variable.get()

    def is_creature(self):
        return self.get() == self.CREATURE

    def invert(self):
        if self.game.paused:
            self.update(self.NOBODY if self.is_creature() else self.CREATURE)

    def make_button(self, command):
        self.button = tk.Button(self.mainframe, textvariable=self.cell_variable,
                                command=command)
        self.reload_button_image()

    def make_invisible(self):
        self.row = self.col = None
        self.button.grid_remove()

    def update(self, value):
        self.set(value)
        self.reload_button_image()

    def reload_button_image(self):
        cell_type = self.cell_variable.get()
        if cell_type in self.images:
            self.button.config(image=self.images[cell_type])
        else:
            self.button.config(height=self.cell_text_size,
                               width=self.cell_text_size)


class GameOfLife:
    DEFAULT_WIDTH = 15
    DEFAULT_HEIGHT = 15
    MAX_SPEED = 50
    MIN_SPEED = 2
    SPEED_STEP = 2
    MIN_DIMENSION = 6

    def __init__(self):
        self.height = self.DEFAULT_HEIGHT
        self.width = self.DEFAULT_WIDTH
        self.speed = 5
        self.paused = False
        self.root = None

        self.init_field()
        self.pause_button = tk.Button(self.mainframe, text="Pause", command=self.pause, width=2)
        self.pause_button.grid(column=1, row=self.height + 2, columnspan=2)

        slow_button = tk.Button(self.mainframe, text="<<", command=self.slow_down, width=1)
        slow_button.grid(column=self.width - 4, row=self.height + 2, columnspan=2)

        fast_button = tk.Button(self.mainframe, text=">>", command=self.speed_up, width=1)
        fast_button.grid(column=self.width - 2, row=self.height + 2, columnspan=2)

    def init_field(self, filename='gameoflife.conf'):
        try:
            self.load_configuration(filename)
        except FileNotFoundError:
            print("No configuration file found at '{}'".format(filename), file=sys.stderr)
            print("Generating random configuration...", file=sys.stderr)
            self.generate_random_config()
        except PermissionError:
            print("Permission to file '{}' denied".format(filename), file=sys.stderr)
            print("Generating random configuration...", file=sys.stderr)
            self.generate_random_config()
        except ValueError as e:
            print("Wrong config format: {}".format(e), file=sys.stderr)
            print("Generating random configuration...", file=sys.stderr)
            self.generate_random_config()

    def update_mainframe(self):
        if not self.root:
            self.root = Tk()
            self.root.title("Game of Life")
            self.root.bind('<Return>', self.recalculate)

        padding_format = "{0} {0} {0} {0}".format(10)
        self.mainframe = ttk.Frame(self.root, padding=padding_format)
        self.mainframe.grid(column=self.width, row=self.height + 2)
        self.mainframe.rowconfigure(self.height + 1, minsize=10)
        self.mainframe.columnconfigure(self.width + 1, minsize=10)

        try:
            self.cell_images = {Cell.CREATURE: PhotoImage(file="img/invader.png"),
                                Cell.NOBODY: PhotoImage(file="img/darkness.png")}
        except OSError as e:
            print("Bad image path: {}".format(e), file=sys.stderr)
            self.cell_images = {}
        except TclError as e:
            print("Bad image provided: {}".format(e), file=sys.stderr)
            self.cell_images = {}

    def recalculate_in_time(self):
        if not self.paused:
            self.recalculate()
        self.root.after(int(1000 / self.speed), self.recalculate_in_time)

    def slow_down(self):
        if (self.speed > self.MIN_SPEED):
            self.speed -= self.SPEED_STEP

    def speed_up(self):
        if (self.speed < self.MAX_SPEED):
            self.speed += self.SPEED_STEP

    def run(self):
        self.recalculate_in_time()
        self.root.mainloop()

    def pause(self):
        self.paused = not self.paused
        self.pause_button.config(text="Run!" if self.paused else "Pause")

    def hide_board_border(self):
        for i in range(len(self.cells[0])):
            self.cells[0][i].make_invisible()
        for i in range(len(self.cells[-1])):
            self.cells[-1][i].make_invisible()
        for i in range(1, len(self.cells) - 1):
            self.cells[i][0].make_invisible()
            self.cells[i][-1].make_invisible()

    def load_configuration(self, filename):
        with open(filename, 'r') as fin:
            self.width, self.height = map(int, fin.readline().split())
            if min(self.width, self.height) < self.MIN_DIMENSION:
                error_msg = "Field dimensions cannot be less than {}".format(self.MIN_DIMENSION)
                raise ValueError(error_msg)
            self.update_mainframe()
            self.cells = [[Cell(self, i, j, self.cell_images) for i in range(self.width + 2)]
                          for j in range(self.height + 2)]

            for row in range(1, self.height + 1):
                values = fin.readline().strip()
                for col in range(1, self.width + 1):
                    if len(values) != self.width:
                        error_msg = "Length of line #{}: {}, expected: {}".format(len(values),
                                                                                  self.width)
                        raise ValueError(error_msg)

                    if (values[col - 1] not in (Cell.CREATURE, Cell.NOBODY)):
                        error_msg = "Bad symbol '{}' in config file".format(values[col - 1])
                        raise ValueError(error_msg)

                    self.cells[row][col].set(values[col - 1])

        self.hide_board_border()

    def generate_random_config(self, width=DEFAULT_WIDTH, height=DEFAULT_HEIGHT):
        self.width = width
        self.height = height
        self.update_mainframe()

        self.cells = [[Cell(self, i, j, self.cell_images) for i in range(width + 2)]
                      for j in range(height + 2)]
        self.buttons = []
        for row in range(1, height + 1):
            self.buttons.append([])
            for col in range(1, width + 1):
                if random.random() > 0.5:
                    self.cells[row][col].set(Cell.CREATURE)

        self.hide_board_border()

    def count_neighbors(self, i, j):
        counter = 0
        delta = (-1, 0, 1)
        for di in delta:
            for dj in delta:
                counter += int(self.cells[i + di][j + dj].is_creature())
        counter -= int(self.cells[i][j].is_creature())
        return counter

    def update_cells(self, new_cells):
        for i in range(1, self.height + 1):
            for j in range(1, self.width + 1):
                self.cells[i][j].update(new_cells[i - 1][j - 1])

    def recalculate(self):
        new_cells = []
        for i in range(1, self.height + 1):
            new_cells.append([])
            for j in range(1, self.width + 1):
                neighbors_number = self.count_neighbors(i, j)
                if self.cells[i][j].is_creature():
                    if neighbors_number not in (2, 3):
                        new_cells[-1].append(Cell.NOBODY)
                        continue
                elif neighbors_number == 3:
                    new_cells[-1].append(Cell.CREATURE)
                    continue
                new_cells[-1].append(self.cells[i][j].get())

        self.update_cells(new_cells)


if __name__ == "__main__":
    game = GameOfLife()
    game.run()
