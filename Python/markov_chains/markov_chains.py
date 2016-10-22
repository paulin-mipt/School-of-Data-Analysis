import re
import argparse
import unittest
from collections import Counter, defaultdict
import random

MAX_RANDOM_ITER = 10000


def tokenize(line):
    return re.findall(r"[^\W\d]+|[^\w\s]+|\s+|\d+", line, re.LOCALE & re.UNICODE)


def output_tokens(input_stream, args):
    line = input_stream.readline()

    with open('output.txt', 'w', encoding='utf-8') as output_stream:
        for token in tokenize(line.strip()):
            print(token, file=output_stream)


class MarkovChainsGenerator:
    def __init__(self, depth):
        self.depth = depth
        self.frequencies = defaultdict(Counter)

    def calculate_probabilities(self, token_generator):
        current_tokens = []
        for token in token_generator:
            for start in range(len(current_tokens) + 1):
                chain = tuple(current_tokens[start:])
                self.frequencies[chain][token] += 1

            if len(current_tokens) == self.depth:
                current_tokens.pop(0)
            current_tokens.append(token)

    def get_probabilities_table(self):
        probabilities_table = {}

        for chain, token_counter in self.frequencies.items():
            size = sum(token_counter.values())
            probabilities_table[' '.join(chain)] = [(token, freq / size)
                                                    for token, freq
                                                    in sorted(token_counter.items())]

        return probabilities_table

    def _make_random_token(self, chain=tuple()):
        token_counter = self.frequencies[chain]

        if not token_counter:
            return None

        size = sum(token_counter.values())
        random_proportion = random.randint(0, size - 1)
        current_proportion = 0
        for token, frequency in token_counter.items():
            current_proportion += frequency
            if random_proportion < current_proportion:
                return token

    def _make_next_token(self, new_sentence_tokens):
        chain = tuple(new_sentence_tokens)
        return self._make_random_token(chain)

    def _make_initial_token(self, max_iterations=MAX_RANDOM_ITER):
        new_token = self._make_random_token()
        iteration_count = 0
        while not new_token.isalpha() and iteration_count < max_iterations:
            new_token = self._make_random_token()

        if iteration_count == max_iterations:
            raise ValueError("unable to choose a random word to start with, "
                             "make sure your text set contains any words "
                             "and try again")

        return new_token

    def _format_token(self, token, previous_token):
        # suppose most punctuators may be followed by space
        # except examples like Anna-Maria, format_token, Hershey's
        if token.isalnum() or token in "([{«<":
            return " {}".format(token) if previous_token.strip().isalnum() else token
        else:
            return "{} ".format(token) if token not in ("-_'/") else token

    def _make_sentence_string(self, tokens):
        sentence = []
        for token in tokens:
            sentence.append(self._format_token(token, sentence[-1]) if sentence else token)
        return ''.join(sentence).strip()

    def generate_text(self, tokens_num):
        sentences = []
        new_sentence_tokens = []
        is_first_sentence_token = True

        for i in range(tokens_num):
            if is_first_sentence_token:
                new_token = self._make_initial_token()
                is_first_sentence_token = False
                new_sentence_tokens.append(new_token)
            else:
                new_token = self._make_next_token(new_sentence_tokens[-self.depth:])
                if new_token is None:
                    if new_sentence_tokens:
                        # capitalize the first word
                        if new_sentence_tokens[0][0].islower():
                            new_sentence_tokens[0] = new_sentence_tokens[0].title()
                        sentences.append(self._make_sentence_string(new_sentence_tokens))
                        new_sentence_tokens = []
                    is_first_sentence_token = True
                else:
                    new_sentence_tokens.append(new_token)

        if new_sentence_tokens:
            if new_sentence_tokens[0][0].islower():
                new_sentence_tokens[0] = new_sentence_tokens[0].title()
            new_sentence_tokens.append('.')
            sentences.append(self._make_sentence_string(new_sentence_tokens))
        return ' '.join(sentences)


def output_probabilities(input_stream, args):
    generator = MarkovChainsGenerator(args.depth)

    for line in input_stream:
        line = line.strip()
        generator.calculate_probabilities(filter(str.isalpha, tokenize(line)))

    with open('output.txt', 'w', encoding='utf-8') as output_stream:
        for chain, tokens in sorted(generator.get_probabilities_table().items()):
            print(chain, file=output_stream)
            for token, probability in tokens:
                print('  {tok}: {prob:.2f}'.format(tok=token, prob=probability),
                      file=output_stream)


def output_generated(input_stream, args):
    generator = MarkovChainsGenerator(args.depth)
    raw_text = input_stream.read()
    generator.calculate_probabilities(filter(lambda x: re.match("\S", x), tokenize(raw_text)))

    with open('output.txt', 'w', encoding='utf-8') as output_stream:
        print(generator.generate_text(args.size), file=output_stream)


class TokenizerTester(unittest.TestCase):
    def test_alphabet(self):
        string = 'Hello world'
        tokens = list(tokenize(string))
        self.assertEqual(tokens, ['Hello', ' ', 'world'])

    def test_digits(self):
        string = 'Hello22 world'
        tokens = list(tokenize(string))
        self.assertEqual(tokens, ['Hello', '22', ' ', 'world'])

    def test_punctuation(self):
        string = 'Hello, world?!'
        tokens = list(tokenize(string))
        self.assertEqual(tokens, ['Hello', ',', ' ', 'world', '?!'])

    def test_composites(self):
        string = "Anna-Maria's cat"
        tokens = list(tokenize(string))
        self.assertEqual(tokens, ["Anna", "-", "Maria", "'", "s", ' ', 'cat'])


class ProbabilitiesTester(unittest.TestCase):
    def test_single_token(self):
        generator = MarkovChainsGenerator(1)
        tokens = ['Hello']
        generator.calculate_probabilities(tokens)
        probabilities_table = generator.get_probabilities_table()
        self.assertEqual(probabilities_table, {'': [('Hello', 1.0)]})

    def test_depth(self):
        generator = MarkovChainsGenerator(2)
        tokens = ['First', 'test', 'sentence', 'test']
        generator.calculate_probabilities(tokens)
        probabilities_table = generator.get_probabilities_table()
        self.assertEqual(probabilities_table, {
            '': [('First', 0.25), ('sentence', 0.25), ('test', 0.5)],
            'First': [('test', 1.0)],
            'First test': [('sentence', 1.0)],
            'sentence': [('test', 1.0)],
            'test': [('sentence', 1.0)],
            'test sentence': [('test', 1.0)]})


def unit_test(*args):
    unittest.main()


def make_argparser():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers()

    parser_tokenize = subparsers.add_parser('tokenize')
    parser_tokenize.set_defaults(handler=output_tokens)

    parser_probabilities = subparsers.add_parser('probabilities')
    parser_probabilities.add_argument('--depth', type=int, required=True, choices=range(1, 21))
    parser_probabilities.set_defaults(handler=output_probabilities)

    parser_generate = subparsers.add_parser('generate')
    parser_generate.add_argument('--depth', type=int, required=True, choices=range(1, 21))
    parser_generate.add_argument('--size', type=int, required=True, choices=range(1, 10000))
    parser_generate.set_defaults(handler=output_generated)

    parser_test = subparsers.add_parser('test')
    parser_test.set_defaults(handler=unit_test)

    return parser


def main():
    parser = make_argparser()

    with open('input.txt', encoding='utf-8') as input_stream:
        args = input_stream.readline()
        args = parser.parse_args(args.split())
        args.handler(input_stream, args)


if __name__ == '__main__':
    main()
