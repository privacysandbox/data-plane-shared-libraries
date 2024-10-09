// Copyright 2009 the V8 project authors. All rights reserved.
// Copyright (C) 2015 Apple Inc. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This benchmark is based on a JavaScript log processing module used
// by the V8 profiler to generate execution time profiles for runs of
// JavaScript applications, and it effectively measures how fast the
// JavaScript engine is at allocating nodes and reclaiming the memory
// used for old nodes. Because of the way splay trees work, the engine
// also has to deal with a lot of changes to the large tree object
// graph.

// Configuration.
var kSplayTreeSize = 8000;
var kSplayTreeModifications = 80;
var kSplayTreePayloadDepth = 5;

var splayTree = null;
var splaySampleTimeStart = 0.0;

function GeneratePayloadTree(depth, tag) {
  if (depth == 0) {
    return {
      array: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9],
      string: 'String for key ' + tag + ' in leaf node',
    };
  } else {
    return {
      left: GeneratePayloadTree(depth - 1, tag),
      right: GeneratePayloadTree(depth - 1, tag),
    };
  }
}

function GenerateKey() {
  // The benchmark framework guarantees that Math.random is
  // deterministic; see base.js.
  return Math.random();
}

var splaySamples = [];

function SplayLatency() {
  return splaySamples;
}

function SplayUpdateStats(time) {
  var pause = time - splaySampleTimeStart;
  splaySampleTimeStart = time;
  splaySamples.push(pause);
}

function InsertNewNode() {
  // Insert new node with a unique key.
  var key;
  do {
    key = GenerateKey();
  } while (splayTree.find(key) != null);
  var payload = GeneratePayloadTree(kSplayTreePayloadDepth, String(key));
  splayTree.insert(key, payload);
  return key;
}

function SplaySetup() {
  // Check if the platform has the performance.now high resolution timer.
  // If not, throw exception and quit.
  if (!performance.now) {
    throw 'PerformanceNowUnsupported';
  }

  splayTree = new SplayTree();
  splaySampleTimeStart = performance.now();
  for (var i = 0; i < kSplayTreeSize; i++) {
    InsertNewNode();
    if ((i + 1) % 20 == 19) {
      SplayUpdateStats(performance.now());
    }
  }
}

function SplayTearDown() {
  // Allow the garbage collector to reclaim the memory
  // used by the splay tree no matter how we exit the
  // tear down function.
  var keys = splayTree.exportKeys();
  splayTree = null;

  splaySamples = [];

  // Verify that the splay tree has the right size.
  var length = keys.length;
  if (length != kSplayTreeSize) {
    throw new Error('Splay tree has wrong size');
  }

  // Verify that the splay tree has sorted, unique keys.
  for (var i = 0; i < length - 1; i++) {
    if (keys[i] >= keys[i + 1]) {
      throw new Error('Splay tree not sorted');
    }
  }
}

function SplayRun() {
  // Replace a few nodes in the splay tree.
  for (var i = 0; i < kSplayTreeModifications; i++) {
    var key = InsertNewNode();
    var greatest = splayTree.findGreatestLessThan(key);
    if (greatest == null) splayTree.remove(key);
    else splayTree.remove(greatest.key);
  }
  SplayUpdateStats(performance.now());
}

/**
 * Constructs a Splay tree.  A splay tree is a self-balancing binary
 * search tree with the additional property that recently accessed
 * elements are quick to access again. It performs basic operations
 * such as insertion, look-up and removal in O(log(n)) amortized time.
 *
 * @constructor
 */
function SplayTree() {}

/**
 * Pointer to the root node of the tree.
 *
 * @type {SplayTree.Node}
 * @private
 */
SplayTree.prototype.root_ = null;

/**
 * @return {boolean} Whether the tree is empty.
 */
SplayTree.prototype.isEmpty = function () {
  return !this.root_;
};

/**
 * Inserts a node into the tree with the specified key and value if
 * the tree does not already contain a node with the specified key. If
 * the value is inserted, it becomes the root of the tree.
 *
 * @param {number} key Key to insert into the tree.
 * @param {*} value Value to insert into the tree.
 */
SplayTree.prototype.insert = function (key, value) {
  if (this.isEmpty()) {
    this.root_ = new SplayTree.Node(key, value);
    return;
  }
  // Splay on the key to move the last node on the search path for
  // the key to the root of the tree.
  this.splay_(key);
  if (this.root_.key == key) {
    return;
  }
  var node = new SplayTree.Node(key, value);
  if (key > this.root_.key) {
    node.left = this.root_;
    node.right = this.root_.right;
    this.root_.right = null;
  } else {
    node.right = this.root_;
    node.left = this.root_.left;
    this.root_.left = null;
  }
  this.root_ = node;
};

/**
 * Removes a node with the specified key from the tree if the tree
 * contains a node with this key. The removed node is returned. If the
 * key is not found, an exception is thrown.
 *
 * @param {number} key Key to find and remove from the tree.
 * @return {SplayTree.Node} The removed node.
 */
SplayTree.prototype.remove = function (key) {
  if (this.isEmpty()) {
    throw Error('Key not found: ' + key);
  }
  this.splay_(key);
  if (this.root_.key != key) {
    throw Error('Key not found: ' + key);
  }
  var removed = this.root_;
  if (!this.root_.left) {
    this.root_ = this.root_.right;
  } else {
    var right = this.root_.right;
    this.root_ = this.root_.left;
    // Splay to make sure that the new root has an empty right child.
    this.splay_(key);
    // Insert the original right child as the right child of the new
    // root.
    this.root_.right = right;
  }
  return removed;
};

/**
 * Returns the node having the specified key or null if the tree doesn't contain
 * a node with the specified key.
 *
 * @param {number} key Key to find in the tree.
 * @return {SplayTree.Node} Node having the specified key.
 */
SplayTree.prototype.find = function (key) {
  if (this.isEmpty()) {
    return null;
  }
  this.splay_(key);
  return this.root_.key == key ? this.root_ : null;
};

/**
 * @return {SplayTree.Node} Node having the maximum key value.
 */
SplayTree.prototype.findMax = function (opt_startNode) {
  if (this.isEmpty()) {
    return null;
  }
  var current = opt_startNode || this.root_;
  while (current.right) {
    current = current.right;
  }
  return current;
};

/**
 * @return {SplayTree.Node} Node having the maximum key value that
 *     is less than the specified key value.
 */
SplayTree.prototype.findGreatestLessThan = function (key) {
  if (this.isEmpty()) {
    return null;
  }
  // Splay on the key to move the node with the given key or the last
  // node on the search path to the top of the tree.
  this.splay_(key);
  // Now the result is either the root node or the greatest node in
  // the left subtree.
  if (this.root_.key < key) {
    return this.root_;
  } else if (this.root_.left) {
    return this.findMax(this.root_.left);
  } else {
    return null;
  }
};

/**
 * @return {Array<*>} An array containing all the keys of tree's nodes.
 */
SplayTree.prototype.exportKeys = function () {
  var result = [];
  if (!this.isEmpty()) {
    this.root_.traverse_(function (node) {
      result.push(node.key);
    });
  }
  return result;
};

/**
 * Perform the splay operation for the given key. Moves the node with
 * the given key to the top of the tree.  If no node has the given
 * key, the last node on the search path is moved to the top of the
 * tree. This is the simplified top-down splaying algorithm from:
 * "Self-adjusting Binary Search Trees" by Sleator and Tarjan
 *
 * @param {number} key Key to splay the tree on.
 * @private
 */
SplayTree.prototype.splay_ = function (key) {
  if (this.isEmpty()) {
    return;
  }
  // Create a dummy node.  The use of the dummy node is a bit
  // counter-intuitive: The right child of the dummy node will hold
  // the L tree of the algorithm.  The left child of the dummy node
  // will hold the R tree of the algorithm.  Using a dummy node, left
  // and right will always be nodes and we avoid special cases.
  var dummy, left, right;
  dummy = left = right = new SplayTree.Node(null, null);
  var current = this.root_;
  while (true) {
    if (key < current.key) {
      if (!current.left) {
        break;
      }
      if (key < current.left.key) {
        // Rotate right.
        var tmp = current.left;
        current.left = tmp.right;
        tmp.right = current;
        current = tmp;
        if (!current.left) {
          break;
        }
      }
      // Link right.
      right.left = current;
      right = current;
      current = current.left;
    } else if (key > current.key) {
      if (!current.right) {
        break;
      }
      if (key > current.right.key) {
        // Rotate left.
        var tmp = current.right;
        current.right = tmp.left;
        tmp.left = current;
        current = tmp;
        if (!current.right) {
          break;
        }
      }
      // Link left.
      left.right = current;
      left = current;
      current = current.right;
    } else {
      break;
    }
  }
  // Assemble.
  left.right = current.left;
  right.left = current.right;
  current.left = dummy.right;
  current.right = dummy.left;
  this.root_ = current;
};

/**
 * Constructs a Splay tree node.
 *
 * @param {number} key Key.
 * @param {*} value Value.
 */
SplayTree.Node = function (key, value) {
  this.key = key;
  this.value = value;
};

/**
 * @type {SplayTree.Node}
 */
SplayTree.Node.prototype.left = null;

/**
 * @type {SplayTree.Node}
 */
SplayTree.Node.prototype.right = null;

/**
 * Performs an ordered traversal of the subtree starting at
 * this SplayTree.Node.
 *
 * @param {function(SplayTree.Node)} f Visitor function.
 * @private
 */
SplayTree.Node.prototype.traverse_ = function (f) {
  var current = this;
  while (current) {
    var left = current.left;
    if (left) left.traverse_(f);
    f(current);
    current = current.right;
  }
};

class CardDeck {
  constructor() {
    this.newDeck();
  }

  newDeck() {
    // Make a shallow copy of a new deck
    this._cards = CardDeck._newDeck.slice(0);
  }

  shuffle() {
    this.newDeck();

    for (let index = 52; index !== 0; ) {
      // Select a random card
      let randomIndex = Math.floor(Math.random() * index);
      index--;

      // Swap the current card with the random card
      let tempCard = this._cards[index];
      this._cards[index] = this._cards[randomIndex];
      this._cards[randomIndex] = tempCard;
    }
  }

  dealOneCard() {
    return this._cards.shift();
  }

  static cardRank(card) {
    // This returns a numeric value for a card.
    // Ace is highest.

    let rankOfCard = card.codePointAt(0) & 0xf;
    if (rankOfCard == 0x1)
      // Make Aces higher than Kings
      rankOfCard = 0xf;

    return rankOfCard;
  }

  static cardName(card) {
    if (typeof card == 'string') card = card.codePointAt(0);
    return this._rankNames[card & 0xf];
  }
}

CardDeck._rankNames = ['', 'Ace', '2', '3', '4', '5', '6', '7', '8', '9', '10', 'Jack', '', 'Queen', 'King'];

CardDeck._newDeck = [
  // Spades
  '\u{1f0a1}',
  '\u{1f0a2}',
  '\u{1f0a3}',
  '\u{1f0a4}',
  '\u{1f0a5}',
  '\u{1f0a6}',
  '\u{1f0a7}',
  '\u{1f0a8}',
  '\u{1f0a9}',
  '\u{1f0aa}',
  '\u{1f0ab}',
  '\u{1f0ad}',
  '\u{1f0ae}',
  // Hearts
  '\u{1f0b1}',
  '\u{1f0b2}',
  '\u{1f0b3}',
  '\u{1f0b4}',
  '\u{1f0b5}',
  '\u{1f0b6}',
  '\u{1f0b7}',
  '\u{1f0b8}',
  '\u{1f0b9}',
  '\u{1f0ba}',
  '\u{1f0bb}',
  '\u{1f0bd}',
  '\u{1f0be}',
  // Clubs
  '\u{1f0d1}',
  '\u{1f0d2}',
  '\u{1f0d3}',
  '\u{1f0d4}',
  '\u{1f0d5}',
  '\u{1f0d6}',
  '\u{1f0d7}',
  '\u{1f0d8}',
  '\u{1f0d9}',
  '\u{1f0da}',
  '\u{1f0db}',
  '\u{1f0dd}',
  '\u{1f0de}',
  // Diamonds
  '\u{1f0c1}',
  '\u{1f0c2}',
  '\u{1f0c3}',
  '\u{1f0c4}',
  '\u{1f0c5}',
  '\u{1f0c6}',
  '\u{1f0c7}',
  '\u{1f0c8}',
  '\u{1f0c9}',
  '\u{1f0ca}',
  '\u{1f0cb}',
  '\u{1f0cd}',
  '\u{1f0ce}',
];

class Hand {
  constructor() {
    this.clear();
  }

  clear() {
    this._cards = [];
    this._rank = 0;
  }

  takeCard(card) {
    this._cards.push(card);
  }

  score() {
    // Sort highest rank to lowest
    this._cards.sort((a, b) => {
      return CardDeck.cardRank(b) - CardDeck.cardRank(a);
    });

    let handString = this._cards.join('');

    let flushResult = handString.match(Hand.FlushRegExp);
    let straightResult = handString.match(Hand.StraightRegExp);
    let ofAKindResult = handString.match(Hand.OfAKindRegExp);

    if (flushResult) {
      if (straightResult) {
        if (straightResult[1]) this._rank = Hand.RoyalFlush;
        else this._rank = Hand.StraightFlush;
      } else this._rank = Hand.Flush;

      this._rank |= (CardDeck.cardRank(this._cards[0]) << 16) | (CardDeck.cardRank(this._cards[1]) << 12);
    } else if (straightResult)
      this._rank =
        Hand.Straight | (CardDeck.cardRank(this._cards[0]) << 16) | (CardDeck.cardRank(this._cards[1]) << 12);
    else if (ofAKindResult) {
      // When comparing lengths, a matched unicode character has a length of 2.
      // Therefore expected lengths are doubled, e.g a pair will have a match length of 4.
      if (ofAKindResult[0].length == 8) this._rank = Hand.FourOfAKind | CardDeck.cardRank(this._cards[0]);
      else {
        // Found pair or three of a kind.  Check for two pair or full house.
        let firstOfAKind = ofAKindResult[0];
        let remainingCardsIndex = handString.indexOf(firstOfAKind) + firstOfAKind.length;
        let secondOfAKindResult;
        if (
          remainingCardsIndex <= 6 &&
          (secondOfAKindResult = handString.slice(remainingCardsIndex).match(Hand.OfAKindRegExp))
        ) {
          if (
            (firstOfAKind.length == 6 && secondOfAKindResult[0].length == 4) ||
            (firstOfAKind.length == 4 && secondOfAKindResult[0].length == 6)
          ) {
            let threeOfAKindCardRank;
            let twoOfAKindCardRank;
            if (firstOfAKind.length == 6) {
              threeOfAKindCardRank = CardDeck.cardRank(firstOfAKind.slice(0, 2));
              twoOfAKindCardRank = CardDeck.cardRank(secondOfAKindResult[0].slice(0, 2));
            } else {
              threeOfAKindCardRank = CardDeck.cardRank(secondOfAKindResult[0].slice(0, 2));
              twoOfAKindCardRank = CardDeck.cardRank(firstOfAKind.slice(0, 2));
            }
            this._rank =
              Hand.FullHouse |
              (threeOfAKindCardRank << 16) |
              (threeOfAKindCardRank < 12) |
              (threeOfAKindCardRank << 8) |
              (twoOfAKindCardRank << 4) |
              twoOfAKindCardRank;
          } else if (firstOfAKind.length == 4 && secondOfAKindResult[0].length == 4) {
            let firstPairCardRank = CardDeck.cardRank(firstOfAKind.slice(0, 2));
            let SecondPairCardRank = CardDeck.cardRank(secondOfAKindResult[0].slice(0, 2));
            let otherCardRank;
            // Due to sorting, the other card is at index 0, 4 or 8
            if (firstOfAKind.codePointAt(0) == handString.codePointAt(0)) {
              if (secondOfAKindResult[0].codePointAt(0) == handString.codePointAt(4))
                otherCardRank = CardDeck.cardRank(handString.slice(8, 10));
              else otherCardRank = CardDeck.cardRank(handString.slice(4, 6));
            } else otherCardRank = CardDeck.cardRank(handString.slice(0, 2));

            this._rank =
              Hand.TwoPair |
              (firstPairCardRank << 16) |
              (firstPairCardRank << 12) |
              (SecondPairCardRank << 8) |
              (SecondPairCardRank << 4) |
              otherCardRank;
          }
        } else {
          let ofAKindCardRank = CardDeck.cardRank(firstOfAKind.slice(0, 2));
          let otherCardsRank = 0;
          for (let card of this._cards) {
            let cardRank = CardDeck.cardRank(card);
            if (cardRank != ofAKindCardRank) otherCardsRank = (otherCardsRank << 4) | cardRank;
          }

          if (firstOfAKind.length == 6)
            this._rank =
              Hand.ThreeOfAKind |
              (ofAKindCardRank << 16) |
              (ofAKindCardRank << 12) |
              (ofAKindCardRank << 8) |
              otherCardsRank;
          else this._rank = Hand.Pair | (ofAKindCardRank << 16) | (ofAKindCardRank << 12) | otherCardsRank;
        }
      }
    } else {
      this._rank = 0;
      for (let card of this._cards) {
        let cardRank = CardDeck.cardRank(card);
        this._rank = (this._rank << 4) | cardRank;
      }
    }
  }

  get rank() {
    return this._rank;
  }

  toString() {
    return this._cards.join('');
  }
}

Hand.FlushRegExp = new RegExp(
  '([\u{1f0a1}-\u{1f0ae}]{5})|([\u{1f0b1}-\u{1f0be}]{5})|([\u{1f0c1}-\u{1f0ce}]{5})|([\u{1f0d1}-\u{1f0de}]{5})',
  'u'
);

Hand.StraightRegExp = new RegExp(
  '([\u{1f0a1}\u{1f0b1}\u{1f0d1}\u{1f0c1}][\u{1f0ae}\u{1f0be}\u{1f0de}\u{1f0ce}][\u{1f0ad}\u{1f0bd}\u{1f0dd}\u{1f0cd}][\u{1f0ab}\u{1f0bb}\u{1f0db}\u{1f0cb}][\u{1f0aa}\u{1f0ba}\u{1f0da}\u{1f0ca}])|[\u{1f0ae}\u{1f0be}\u{1f0de}\u{1f0ce}][\u{1f0ad}\u{1f0bd}\u{1f0dd}\u{1f0cd}][\u{1f0ab}\u{1f0bb}\u{1f0db}\u{1f0cb}][\u{1f0aa}\u{1f0ba}\u{1f0da}\u{1f0ca}][\u{1f0a9}\u{1f0b9}\u{1f0d9}\u{1f0c9}]|[\u{1f0ad}\u{1f0bd}\u{1f0dd}\u{1f0cd}][\u{1f0ab}\u{1f0bb}\u{1f0db}\u{1f0cb}][\u{1f0aa}\u{1f0ba}\u{1f0da}\u{1f0ca}][\u{1f0a9}\u{1f0b9}\u{1f0d9}\u{1f0c9}][\u{1f0a8}\u{1f0b8}\u{1f0d8}\u{1f0c8}]|[\u{1f0ab}\u{1f0bb}\u{1f0db}\u{1f0cb}][\u{1f0aa}\u{1f0ba}\u{1f0da}\u{1f0ca}][\u{1f0a9}\u{1f0b9}\u{1f0d9}\u{1f0c9}][\u{1f0a8}\u{1f0b8}\u{1f0d8}\u{1f0c8}][\u{1f0a7}\u{1f0b7}\u{1f0d7}\u{1f0c7}]|[\u{1f0aa}\u{1f0ba}\u{1f0da}\u{1f0ca}][\u{1f0a9}\u{1f0b9}\u{1f0d9}\u{1f0c9}][\u{1f0a8}\u{1f0b8}\u{1f0d8}\u{1f0c8}][\u{1f0a7}\u{1f0b7}\u{1f0d7}\u{1f0c7}][\u{1f0a6}\u{1f0b6}\u{1f0d6}\u{1f0c6}]|[\u{1f0a9}\u{1f0b9}\u{1f0d9}\u{1f0c9}][\u{1f0a8}\u{1f0b8}\u{1f0d8}\u{1f0c8}][\u{1f0a7}\u{1f0b7}\u{1f0d7}\u{1f0c7}][\u{1f0a6}\u{1f0b6}\u{1f0d6}\u{1f0c6}][\u{1f0a5}\u{1f0b5}\u{1f0d5}\u{1f0c5}]|[\u{1f0a8}\u{1f0b8}\u{1f0d8}\u{1f0c8}][\u{1f0a7}\u{1f0b7}\u{1f0d7}\u{1f0c7}][\u{1f0a6}\u{1f0b6}\u{1f0d6}\u{1f0c6}][\u{1f0a5}\u{1f0b5}\u{1f0d5}\u{1f0c5}][\u{1f0a4}\u{1f0b4}\u{1f0d4}\u{1f0c4}]|[\u{1f0a7}\u{1f0b7}\u{1f0d7}\u{1f0c7}][\u{1f0a6}\u{1f0b6}\u{1f0d6}\u{1f0c6}][\u{1f0a5}\u{1f0b5}\u{1f0d5}\u{1f0c5}][\u{1f0a4}\u{1f0b4}\u{1f0d4}\u{1f0c4}][\u{1f0a3}\u{1f0b3}\u{1f0d3}\u{1f0c3}]|[\u{1f0a6}\u{1f0b6}\u{1f0d6}\u{1f0c6}][\u{1f0a5}\u{1f0b5}\u{1f0d5}\u{1f0c5}][\u{1f0a4}\u{1f0b4}\u{1f0d4}\u{1f0c4}][\u{1f0a3}\u{1f0b3}\u{1f0d3}\u{1f0c3}][\u{1f0a2}\u{1f0b2}\u{1f0d2}\u{1f0c2}]|[\u{1f0a1}\u{1f0b1}\u{1f0d1}\u{1f0c1}][\u{1f0a5}\u{1f0b5}\u{1f0d5}\u{1f0c5}][\u{1f0a4}\u{1f0b4}\u{1f0d4}\u{1f0c4}][\u{1f0a3}\u{1f0b3}\u{1f0d3}\u{1f0c3}][\u{1f0a2}\u{1f0b2}\u{1f0d2}\u{1f0c2}]',
  'u'
);

Hand.OfAKindRegExp = new RegExp(
  '(?:[\u{1f0a1}\u{1f0b1}\u{1f0d1}\u{1f0c1}]{2,4})|(?:[\u{1f0ae}\u{1f0be}\u{1f0de}\u{1f0ce}]{2,4})|(?:[\u{1f0ad}\u{1f0bd}\u{1f0dd}\u{1f0cd}]{2,4})|(?:[\u{1f0ab}\u{1f0bb}\u{1f0db}\u{1f0cb}]{2,4})|(?:[\u{1f0aa}\u{1f0ba}\u{1f0da}\u{1f0ca}]{2,4})|(?:[\u{1f0a9}\u{1f0b9}\u{1f0d9}\u{1f0c9}]{2,4})|(?:[\u{1f0a8}\u{1f0b8}\u{1f0d8}\u{1f0c8}]{2,4})|(?:[\u{1f0a7}\u{1f0b7}\u{1f0d7}\u{1f0c7}]{2,4})|(?:[\u{1f0a6}\u{1f0b6}\u{1f0d6}\u{1f0c6}]{2,4})|(?:[\u{1f0a5}\u{1f0b5}\u{1f0d5}\u{1f0c5}]{2,4})|(?:[\u{1f0a4}\u{1f0b4}\u{1f0d4}\u{1f0c4}]{2,4})|(?:[\u{1f0a3}\u{1f0b3}\u{1f0d3}\u{1f0c3}]{2,4})|(?:[\u{1f0a2}\u{1f0b2}\u{1f0d2}\u{1f0c2}]{2,4})',
  'u'
);

Hand.RoyalFlush = 0x900000;
Hand.StraightFlush = 0x800000;
Hand.FourOfAKind = 0x700000;
Hand.FullHouse = 0x600000;
Hand.Flush = 0x500000;
Hand.Straight = 0x400000;
Hand.ThreeOfAKind = 0x300000;
Hand.TwoPair = 0x200000;
Hand.Pair = 0x100000;

class Player extends Hand {
  constructor(name) {
    super();
    this._name = name;
    this._wins = 0;
    this._handTypeCounts = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
  }

  scoreHand() {
    this.score();
    let handType = this.rank >> 20;
    this._handTypeCounts[handType]++;
  }

  wonHand() {
    this._wins++;
  }

  get name() {
    return this._name;
  }

  get hand() {
    return super.toString();
  }

  get wins() {
    return this._wins;
  }

  get handTypeCounts() {
    return this._handTypeCounts;
  }
}

function playHands(nHands) {
  const n_players = 10;
  const players = Array.from({ length: n_players }, (i) => new Player(`foo{i}`));
  let cardDeck = new CardDeck();
  let handsPlayed = 0;
  let highestRank = 0;

  do {
    cardDeck.shuffle();

    for (let player of players) player.clear();

    for (let i = 0; i < 5; i++) {
      for (let player of players) player.takeCard(cardDeck.dealOneCard());
    }

    for (let player of players) player.scoreHand();

    handsPlayed++;

    highestRank = 0;

    for (let player of players) {
      if (player.rank > highestRank) highestRank = player.rank;
    }

    for (let player of players) {
      // We count ties as wins for each player.
      if (player.rank == highestRank) player.wonHand();
    }
  } while (handsPlayed < nHands);
}

var solver = null;
var nsFrameCounter = 0;

function runNavierStokes() {
  solver.update();
  nsFrameCounter++;

  if (nsFrameCounter == 15) checkResult(solver.getDens());
}

function checkResult(dens) {
  this.result = 0;
  for (var i = 7000; i < 7100; i++) {
    this.result += ~~(dens[i] * 10);
  }

  if (this.result != 77) {
    throw new Error('checksum failed');
  }
}

function setupNavierStokes() {
  solver = new FluidField(null);
  solver.setResolution(128, 128);
  solver.setIterations(20);
  solver.setDisplayFunction(function () {});
  solver.setUICallback(prepareFrame);
  solver.reset();
}

function tearDownNavierStokes() {
  solver = null;
}

function addPoints(field) {
  var n = 64;
  for (var i = 1; i <= n; i++) {
    field.setVelocity(i, i, n, n);
    field.setDensity(i, i, 5);
    field.setVelocity(i, n - i, -n, -n);
    field.setDensity(i, n - i, 20);
    field.setVelocity(128 - i, n + i, -n, -n);
    field.setDensity(128 - i, n + i, 30);
  }
}

var framesTillAddingPoints = 0;
var framesBetweenAddingPoints = 5;

function prepareFrame(field) {
  if (framesTillAddingPoints == 0) {
    addPoints(field);
    framesTillAddingPoints = framesBetweenAddingPoints;
    framesBetweenAddingPoints++;
  } else {
    framesTillAddingPoints--;
  }
}

// Code from Oliver Hunt (http://nerget.com/fluidSim/pressure.js) starts here.
function FluidField(canvas) {
  function addFields(x, s, dt) {
    for (var i = 0; i < size; i++) x[i] += dt * s[i];
  }

  function set_bnd(b, x) {
    if (b === 1) {
      for (var i = 1; i <= width; i++) {
        x[i] = x[i + rowSize];
        x[i + (height + 1) * rowSize] = x[i + height * rowSize];
      }

      for (var j = 1; j <= height; j++) {
        x[j * rowSize] = -x[1 + j * rowSize];
        x[width + 1 + j * rowSize] = -x[width + j * rowSize];
      }
    } else if (b === 2) {
      for (var i = 1; i <= width; i++) {
        x[i] = -x[i + rowSize];
        x[i + (height + 1) * rowSize] = -x[i + height * rowSize];
      }

      for (var j = 1; j <= height; j++) {
        x[j * rowSize] = x[1 + j * rowSize];
        x[width + 1 + j * rowSize] = x[width + j * rowSize];
      }
    } else {
      for (var i = 1; i <= width; i++) {
        x[i] = x[i + rowSize];
        x[i + (height + 1) * rowSize] = x[i + height * rowSize];
      }

      for (var j = 1; j <= height; j++) {
        x[j * rowSize] = x[1 + j * rowSize];
        x[width + 1 + j * rowSize] = x[width + j * rowSize];
      }
    }
    var maxEdge = (height + 1) * rowSize;
    x[0] = 0.5 * (x[1] + x[rowSize]);
    x[maxEdge] = 0.5 * (x[1 + maxEdge] + x[height * rowSize]);
    x[width + 1] = 0.5 * (x[width] + x[width + 1 + rowSize]);
    x[width + 1 + maxEdge] = 0.5 * (x[width + maxEdge] + x[width + 1 + height * rowSize]);
  }

  function lin_solve(b, x, x0, a, c) {
    if (a === 0 && c === 1) {
      for (var j = 1; j <= height; j++) {
        var currentRow = j * rowSize;
        ++currentRow;
        for (var i = 0; i < width; i++) {
          x[currentRow] = x0[currentRow];
          ++currentRow;
        }
      }
      set_bnd(b, x);
    } else {
      var invC = 1 / c;
      for (var k = 0; k < iterations; k++) {
        for (var j = 1; j <= height; j++) {
          var lastRow = (j - 1) * rowSize;
          var currentRow = j * rowSize;
          var nextRow = (j + 1) * rowSize;
          var lastX = x[currentRow];
          ++currentRow;
          for (var i = 1; i <= width; i++)
            lastX = x[currentRow] =
              (x0[currentRow] + a * (lastX + x[++currentRow] + x[++lastRow] + x[++nextRow])) * invC;
        }
        set_bnd(b, x);
      }
    }
  }

  function diffuse(b, x, x0, dt) {
    var a = 0;
    lin_solve(b, x, x0, a, 1 + 4 * a);
  }

  function lin_solve2(x, x0, y, y0, a, c) {
    if (a === 0 && c === 1) {
      for (var j = 1; j <= height; j++) {
        var currentRow = j * rowSize;
        ++currentRow;
        for (var i = 0; i < width; i++) {
          x[currentRow] = x0[currentRow];
          y[currentRow] = y0[currentRow];
          ++currentRow;
        }
      }
      set_bnd(1, x);
      set_bnd(2, y);
    } else {
      var invC = 1 / c;
      for (var k = 0; k < iterations; k++) {
        for (var j = 1; j <= height; j++) {
          var lastRow = (j - 1) * rowSize;
          var currentRow = j * rowSize;
          var nextRow = (j + 1) * rowSize;
          var lastX = x[currentRow];
          var lastY = y[currentRow];
          ++currentRow;
          for (var i = 1; i <= width; i++) {
            lastX = x[currentRow] = (x0[currentRow] + a * (lastX + x[currentRow] + x[lastRow] + x[nextRow])) * invC;
            lastY = y[currentRow] =
              (y0[currentRow] + a * (lastY + y[++currentRow] + y[++lastRow] + y[++nextRow])) * invC;
          }
        }
        set_bnd(1, x);
        set_bnd(2, y);
      }
    }
  }

  function diffuse2(x, x0, y, y0, dt) {
    var a = 0;
    lin_solve2(x, x0, y, y0, a, 1 + 4 * a);
  }

  function advect(b, d, d0, u, v, dt) {
    var Wdt0 = dt * width;
    var Hdt0 = dt * height;
    var Wp5 = width + 0.5;
    var Hp5 = height + 0.5;
    for (var j = 1; j <= height; j++) {
      var pos = j * rowSize;
      for (var i = 1; i <= width; i++) {
        var x = i - Wdt0 * u[++pos];
        var y = j - Hdt0 * v[pos];
        if (x < 0.5) x = 0.5;
        else if (x > Wp5) x = Wp5;
        var i0 = x | 0;
        var i1 = i0 + 1;
        if (y < 0.5) y = 0.5;
        else if (y > Hp5) y = Hp5;
        var j0 = y | 0;
        var j1 = j0 + 1;
        var s1 = x - i0;
        var s0 = 1 - s1;
        var t1 = y - j0;
        var t0 = 1 - t1;
        var row1 = j0 * rowSize;
        var row2 = j1 * rowSize;
        d[pos] = s0 * (t0 * d0[i0 + row1] + t1 * d0[i0 + row2]) + s1 * (t0 * d0[i1 + row1] + t1 * d0[i1 + row2]);
      }
    }
    set_bnd(b, d);
  }

  function project(u, v, p, div) {
    var h = -0.5 / Math.sqrt(width * height);
    for (var j = 1; j <= height; j++) {
      var row = j * rowSize;
      var previousRow = (j - 1) * rowSize;
      var prevValue = row - 1;
      var currentRow = row;
      var nextValue = row + 1;
      var nextRow = (j + 1) * rowSize;
      for (var i = 1; i <= width; i++) {
        div[++currentRow] = h * (u[++nextValue] - u[++prevValue] + v[++nextRow] - v[++previousRow]);
        p[currentRow] = 0;
      }
    }
    set_bnd(0, div);
    set_bnd(0, p);

    lin_solve(0, p, div, 1, 4);
    var wScale = 0.5 * width;
    var hScale = 0.5 * height;
    for (var j = 1; j <= height; j++) {
      var prevPos = j * rowSize - 1;
      var currentPos = j * rowSize;
      var nextPos = j * rowSize + 1;
      var prevRow = (j - 1) * rowSize;
      var currentRow = j * rowSize;
      var nextRow = (j + 1) * rowSize;

      for (var i = 1; i <= width; i++) {
        u[++currentPos] -= wScale * (p[++nextPos] - p[++prevPos]);
        v[currentPos] -= hScale * (p[++nextRow] - p[++prevRow]);
      }
    }
    set_bnd(1, u);
    set_bnd(2, v);
  }

  function dens_step(x, x0, u, v, dt) {
    addFields(x, x0, dt);
    diffuse(0, x0, x, dt);
    advect(0, x, x0, u, v, dt);
  }

  function vel_step(u, v, u0, v0, dt) {
    addFields(u, u0, dt);
    addFields(v, v0, dt);
    var temp = u0;
    u0 = u;
    u = temp;
    var temp = v0;
    v0 = v;
    v = temp;
    diffuse2(u, u0, v, v0, dt);
    project(u, v, u0, v0);
    var temp = u0;
    u0 = u;
    u = temp;
    var temp = v0;
    v0 = v;
    v = temp;
    advect(1, u, u0, u0, v0, dt);
    advect(2, v, v0, u0, v0, dt);
    project(u, v, u0, v0);
  }
  var uiCallback = function (d, u, v) {};

  function Field(dens, u, v) {
    // Just exposing the fields here rather than using accessors is a measurable win during display (maybe 5%)
    // but makes the code ugly.
    this.setDensity = function (x, y, d) {
      dens[x + 1 + (y + 1) * rowSize] = d;
    };
    this.getDensity = function (x, y) {
      return dens[x + 1 + (y + 1) * rowSize];
    };
    this.setVelocity = function (x, y, xv, yv) {
      u[x + 1 + (y + 1) * rowSize] = xv;
      v[x + 1 + (y + 1) * rowSize] = yv;
    };
    this.getXVelocity = function (x, y) {
      return u[x + 1 + (y + 1) * rowSize];
    };
    this.getYVelocity = function (x, y) {
      return v[x + 1 + (y + 1) * rowSize];
    };
    this.width = function () {
      return width;
    };
    this.height = function () {
      return height;
    };
  }
  function queryUI(d, u, v) {
    for (var i = 0; i < size; i++) u[i] = v[i] = d[i] = 0.0;
    uiCallback(new Field(d, u, v));
  }

  this.update = function () {
    queryUI(dens_prev, u_prev, v_prev);
    vel_step(u, v, u_prev, v_prev, dt);
    dens_step(dens, dens_prev, u, v, dt);
    displayFunc(new Field(dens, u, v));
  };
  this.setDisplayFunction = function (func) {
    displayFunc = func;
  };

  this.iterations = function () {
    return iterations;
  };
  this.setIterations = function (iters) {
    if (iters > 0 && iters <= 100) iterations = iters;
  };
  this.setUICallback = function (callback) {
    uiCallback = callback;
  };
  var iterations = 10;
  var visc = 0.5;
  var dt = 0.1;
  var dens;
  var dens_prev;
  var u;
  var u_prev;
  var v;
  var v_prev;
  var width;
  var height;
  var rowSize;
  var size;
  var displayFunc;
  function reset() {
    rowSize = width + 2;
    size = (width + 2) * (height + 2);
    dens = new Array(size);
    dens_prev = new Array(size);
    u = new Array(size);
    u_prev = new Array(size);
    v = new Array(size);
    v_prev = new Array(size);
    for (var i = 0; i < size; i++) dens_prev[i] = u_prev[i] = v_prev[i] = dens[i] = u[i] = v[i] = 0;
  }
  this.reset = reset;
  this.getDens = function () {
    return dens;
  };
  this.setResolution = function (hRes, wRes) {
    var res = wRes * hRes;
    if (res > 0 && res < 1000000 && (wRes != width || hRes != height)) {
      width = wRes;
      height = hRes;
      reset();
      return true;
    }
    return false;
  };
  this.setResolution(64, 64);
}

function NavierStokes() {
  setupNavierStokes();
  runNavierStokes();
}

SplaySetup();

function StressTest(iters, ns_iters) {
  var start = performance.now();
  iters = iters ?? 100000;
  ns_iters = ns_iters ?? 10;

  for (let i = 0; i < iters; ++i) {
    SplayRun();
  }
  playHands(iters);
  for (let i = 0; i < ns_iters ?? 10; i++) {
    NavierStokes();
  }
  var end = performance.now();
  return `${end - start} ms`;
}
