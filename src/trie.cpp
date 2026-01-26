#include "trie.h"
#include <iostream>

namespace Trie {
    TrieNode::TrieNode() {
        isLeaf = false;
        for(size_t i = 0; i < 26; ++i) children[i] = nullptr;
    }
    TrieNode::~TrieNode() {
        delete[] children;
    }

    void insert(TrieNode* root, const std::string &key) {
        TrieNode* curr = root;
        for(const char &c: key) {
            if(curr->children[(c-'a')] == nullptr) {
                TrieNode* new_node = new TrieNode();
                curr->children[(c-'a')] = new_node;
            } 
            curr = curr->children[(c-'a')];
        }
        curr->isLeaf = true;
    }

    bool search(TrieNode* root, const std::string &key) {
        TrieNode* curr = root;
        for(const char &c: key) {
            if(curr->children[(c-'a')] == nullptr) return false;
            curr = curr->children[(c-'a')];
        }
        return curr->isLeaf;
    }

    bool isPrefix(TrieNode* root, const std::string &key) {
        TrieNode* curr = root;
        for(const char &c: key) {
            if(curr->children[(c-'a')] == nullptr) return false;
            curr = curr->children[(c-'a')];
        }
        return true;
    }

    void findAllWords(TrieNode* node, std::string currendWord, std::vector<std::string> &words) {
        if(node->isLeaf){
            words.push_back(currendWord);
        }
        for(size_t i = 0; i < 26 ; ++i) {
            if(node->children[i]) {
                char c = 'a' + i;
                findAllWords(node->children[i], currendWord + c, words);
            }
        }
    }

    std::vector<std::string> autoComplete(TrieNode* root, const std::string &key){
        TrieNode* curr = root;
        for(const char &c: key) {
            if(curr->children[(c-'a')] == nullptr) return {};
            curr = curr->children[(c-'a')];
        }
        std::vector<std::string> words;
        findAllWords(curr, "", words);
        return words;
    }
}