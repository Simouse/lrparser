#include "src/parser/LRParser.h"

#include <cassert>
#include <stack>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/automata/Automaton.h"
#include "src/common.h"
#include "src/grammar/Grammar.h"
#include "src/grammar/GrammarReader.h"
#include "src/util/BitSet.h"
#include "src/util/Formatter.h"
#include "src/util/TokenReader.h"

namespace gram {

// Helper of automaton creation from LR0 grammar,
// which generates a description for the state. e.g.
// S -> aB : S0 = {S -> .aB, S -> a.B, S -> aB.}
static String createStateName(const gram::Grammar &G, const gram::Symbol &N,
                              const std::vector<SymbolID> &productionBody,
                              int dotPos) {
    assert(static_cast<size_t>(dotPos) <= productionBody.size());

    const auto &symbols = G.getAllSymbols();
    String s = N.name;
    s += " ->";

    int i = 0;
    for (; i < dotPos; ++i) {
        s += ' ';
        s += symbols[productionBody[i]].name;
    }
    s += ' ';
    s += Grammar::SignStrings::dot;
    for (; i < static_cast<int>(productionBody.size()); ++i) {
        s += ' ';
        s += symbols[productionBody[i]].name;
    }
    return s;
}

void LRParser::buildNFA() {
    Automaton &M = this->nfa;
    const Grammar &G = this->gram;
    const auto &symbols = G.getAllSymbols();

    // Store links here, so we can demonstrate how to add epsilon
    // transitions from each state which wants N to all states
    // that generates N eventually.
    // { stateid => nid }
    std::unordered_map<StateID, int> epsilonLinks;

    // Store states which begin the given symbol's productions.
    // { nid => vector<stateid> }
    std::unordered_map<int, std::vector<StateID>> prodStartStates;

    // Store states which finish the given symbol's productions.
    // { nid => vector<stateid> }
    std::unordered_map<int, std::vector<StateID>> prodFinishStates;

    // First we need to copy all actions by order, so symbols and
    // actions are paired.
    for (auto const &symbol : symbols) {
        M.addAction(symbol.name);
    }

    constexpr const char *outFilePrefix = "build_nfa";

    // States from grammar
    auto const &productionTable = G.getProductionTable();
    for (int i = 0; i < static_cast<int>(productionTable.size()); ++i) {
        ProductionID productionId{i};
        auto const &production = productionTable[i];
        auto const &rightSymbols = production.rightSymbols;
        std::vector<StateID> stateIds(rightSymbols.size() + 1);
        for (int dotPos = 0; dotPos <= static_cast<int>(rightSymbols.size());
             ++dotPos) {
            String name = createStateName(G, symbols[production.leftSymbol],
                                          rightSymbols, dotPos);
            StateID stateId = M.addState(name);
            stateIds[dotPos] = stateId;
        }
        // Link states
        for (int dotPos = 0; dotPos < static_cast<int>(rightSymbols.size());
             ++dotPos) {
            StateID thisState{stateIds[dotPos]};
            StateID nextState{stateIds[dotPos + 1]};
            auto &nextSymbol = symbols[rightSymbols[dotPos]];
            M.addTransition(thisState, nextState,
                            static_cast<ActionID>(nextSymbol.id));

            // Which non-terminal symbol do I want to link to?
            if (nextSymbol.type == gram::SymbolType::NON_TERM) {
                epsilonLinks.emplace(thisState, nextSymbol.id);
            }
        }

        // Which states can provide my desired symbol?
        prodStartStates[production.leftSymbol].push_back(stateIds.front());
        prodFinishStates[production.leftSymbol].push_back(stateIds.back());

        // Record states with their reducible productions
        reduceMap[stateIds.back()] = productionId;

        String displayInfo =
            "Add states by production: " + gram.dumpProduction(productionId);
        display(AUTOMATON, INFO, displayInfo.c_str(), &M,
                (void *)outFilePrefix);
    }

    // Start state: Create S' for S
    // Since name cannot contain a `'` in it, `S'` is legit
    {
        auto dotSign = Grammar::SignStrings::dot;
        auto &start = G.getStartSymbol();
        String name = start.name + '\'';
        StateID s0 = M.addState(name + " -> " + dotSign + " " + start.name);
        M.markStartState(s0);
        StateID s1 = M.addState(name + " -> " + start.name + " " + dotSign);
        M.addTransition(s0, s1, static_cast<ActionID>(start.id));
        M.markStartState(s0);
        epsilonLinks.emplace(s0, start.id);
        this->extendedStart = s0;
        this->extendedEnd = s1;
    }

    display(AUTOMATON, INFO, "Create new start and end for augmented grammar",
            &M, (void *)outFilePrefix);

    // Mark final states
    // The last state of augmented start symbol production is a final state.
    M.markFinalState(this->extendedEnd);

    // If a non-terminal has a `$` in its follow set,
    // all productions which generate this symbol will be marked final.
    for (auto const &symbol : symbols) {
        if (symbol.followSet.find(G.getEndOfInputSymbol().id) ==
            symbol.followSet.end()) {
            continue;
        }
        // So this symbol can be accepted.
        // Mark all states that can already generate this symbol.
        for (auto state : prodFinishStates[symbol.id]) {
            M.markFinalState(state);
        }
    }

    display(AUTOMATON, INFO, "Mark final states", &M, (void *)outFilePrefix);

    // Creating epsilon edge for each symbol
    for (const auto &link : epsilonLinks) {
        StateID from = link.first;
        for (StateID to : prodStartStates.at(link.second)) {
            M.addEpsilonTransition(from, to);
        }

        String description =
            "Create epsilon edges for state " + std::to_string(from);
        display(AUTOMATON, INFO, description.c_str(), &M,
                (void *)outFilePrefix);
    }

    display(AUTOMATON, INFO, "NFA is built", &M, (void *)outFilePrefix);
}

void LRParser::buildDFA() {
    dfa = nfa.toDFA();
    display(AUTOMATON, INFO, "DFA is built", &dfa, (void *)"build_dfa");
}

void LRParser::buildParseTables() {
    // Now DFA has complete information
    // 1. DFA has epsilon action in its action vector, so we need to skip it.
    // 2. DFA doesn't have a '$' action, and we need to process it specially.

    auto const &states = dfa.getAllStates();
    auto const &closures = dfa.getClosures();
    auto const &formerStates = dfa.getFormerStates();
    auto const &symbols = gram.getAllSymbols();

    auto nActions = static_cast<int>(dfa.getAllActions().size());
    auto nDFAStates = static_cast<int>(states.size());

    // Shift and Goto items
    for (const auto &state : states) {
        auto const &tranSet = state.tranSet;
        for (auto const &tran : tranSet) {
            ParseAction item{(symbols[tran.action].type == SymbolType::NON_TERM)
                                 ? ParseAction::GOTO
                                 : ParseAction::SHIFT,
                             tran.to};
            addParseTableEntry(state.id, tran.action, item);
        }
    }

    // Process "reduce"
    // Bitset for reducible NFA states (so we can know if a DFA state is
    // reducible)
    util::BitSet mask{formerStates.size()};

    for (auto const &entry : reduceMap) {
        mask.add(entry.first);
    }

    for (StateID state{0}; state < nDFAStates; state = StateID{state + 1}) {
        auto dfaStateBitmap = closures[state].states;  // Copy a bitmap
        dfaStateBitmap &= mask;
        for (auto s : dfaStateBitmap) {
            auto nfaState = static_cast<StateID>(s);
            for (ActionID action{0}; action < nActions;
                 action = ActionID{action + 1}) {
                // Check if the action is allowed by followSet
                ProductionID prodID = reduceMap.at(nfaState);

                addParseTableEntry(state, action,
                                   ParseAction{ParseAction::REDUCE, prodID});
            }
        }
    }

    // Process end of input
    auto endOfInput = static_cast<ActionID>(gram.getEndOfInputSymbol().id);
    for (int i = 0; i < nDFAStates; ++i) {
        if (closures[i].states.contains(this->extendedEnd)) {
            addParseTableEntry(StateID{i}, endOfInput,
                               ParseAction{ParseAction::SUCCESS, -1});
        }
    }

    display(PARSE_TABLE, INFO, "Parse table", this);
}

void LRParser::addParseTableEntry(StateID state, ActionID act,
                                  ParseAction pact) {
    using std::vector;
    if (!canAddParseTableEntry(state, act, pact)) {
        return;
    }

    if (parseTable.empty()) {
        parseTable = vector<vector<vector<ParseAction>>>(
            dfa.getAllStates().size(),
            vector<vector<ParseAction>>(gram.getAllSymbols().size()));
    }
    parseTable[state][act].push_back(pact);
}

String LRParser::dumpParseTableEntry(StateID state, ActionID action) const {
    auto const &items = parseTable.at(state).at(action);
    String s;
    bool commaFlag = false;
    for (auto const &item : items) {
        if (commaFlag) s += ',';
        commaFlag = true;
        using Type = ParseAction::Type;
        switch (item.type) {
            case Type::SUCCESS:
                s += "acc";
                break;
            case Type::GOTO:
                s += std::to_string(item.dest);
                break;
            case Type::SHIFT:
                s += 's';
                s += std::to_string(item.dest);
                break;
            case Type::REDUCE:
                s += 'r';
                s += std::to_string(item.productionID);
                break;
            default:
                throw std::runtime_error(
                    "dumpParseTableEntry(): Unknown parse action type");
        }
    }
    return s;
}

void LRParser::readSymbol(util::TokenReader &reader) {
    String s;
    if (reader.getToken(s)) {
        auto const &symbol = gram.findSymbol(s);
        if (symbol.id == gram.getEpsilonSymbol().id) {
            throw std::runtime_error("Epsilon cannot be used in input");
        } else if (symbol.id == gram.getEndOfInputSymbol().id) {
            inputFlag = false;
        }
        // push_back() should be equivalent to push_front(), since queue
        // is empty
        InputQueue.push_back(symbol.id);
    } else {
        inputFlag = false;
        InputQueue.push_back(gram.getEndOfInputSymbol().id);
    }
}

// Test given stream with parsed results
bool LRParser::test(std::istream &stream) try {
    inputFlag = true;
    stateStack.clear();
    symbolStack.clear();
    InputQueue.clear();
    stateStack.push_back(dfa.getStartState());

    // Only one of them is used
    GrammarReader grammarReader(stream);
    util::TokenReader tokenReader(stream);
    util::TokenReader &reader = launchArgs.strict ? grammarReader : tokenReader;

    if (launchArgs.exhaustInput) {
        display(LOG, INFO,
                "Please input symbols for test (Use '$' to end the input):");
        while (inputFlag) {
            readSymbol(reader);
        }
    }

    display(PARSE_STATES, INFO, "Parse states", this);

    if (!launchArgs.exhaustInput) {
        display(LOG, INFO,
                "Please input symbols for test (Use '$' to end the input):");
    }

    util::Formatter f;

    while (true) {
        if (InputQueue.empty() && inputFlag) {
            readSymbol(reader);
        }

        if (InputQueue.empty())
            throw std::logic_error(
                "No next symbol to use, this shouldn't be possible");

        auto const &tableEntry =
            parseTable[stateStack.back()][InputQueue.front()];

        auto choices = tableEntry.size();
        if (choices <= 0)
            throw std::runtime_error(
                "No viable action in parse table for this input");
        if (choices > 1) {
            throw std::runtime_error(
                "Multiple viable choices. Cannot decide which action "
                "to take");
        }

        // Take action
        auto const &decision = tableEntry.front();
        switch (decision.type) {
            case ParseAction::GOTO:
            case ParseAction::SHIFT:
                stateStack.push_back(decision.dest);
                symbolStack.push_back(InputQueue.front());
                InputQueue.pop_front();
                display(LOG, VERBOSE,
                        decision.type == ParseAction::GOTO
                            ? "Apply GOTO rule"
                            : "Apply SHIFT rule");
                break;
            case ParseAction::REDUCE:
                reduce(decision.productionID);
                display(LOG, VERBOSE,
                        f.formatView("Apply REDUCE by production: %d",
                                     decision.productionID)
                            .data());
                break;
            case ParseAction::SUCCESS:
                display(LOG, INFO, "Success");
                return true;
        }

        display(PARSE_STATES, INFO, "Parse states", this);
    }
    throw UnreachableCodeError();
} catch (std::exception const &e) {
    display(LOG, ERR, e.what());
    return false;
}

// May throw errors
void LRParser::reduce(ProductionID prodID) {
    auto const &prod = gram.getProductionTable()[prodID];
    auto bodySize = prod.rightSymbols.size();
    if (symbolStack.size() < bodySize) {
        throw std::runtime_error(
            "Stack's symbols are not enough for reduction");
    }
    if (stateStack.size() < bodySize) {
        throw std::runtime_error("Stack's states are not enough for reduction");
    }
    auto symbolStackOffset = symbolStack.size() - prod.rightSymbols.size();
    for (size_t i = 0; i < bodySize; ++i) {
        if (symbolStack[symbolStackOffset + i] != prod.rightSymbols[i]) {
            throw std::runtime_error(
                "Stack's symbols cannot fit production body for reduction");
        }
    }
    // Pop those states by production
    for (size_t i = 0; i < bodySize; ++i) {
        symbolStack.pop_back();
        stateStack.pop_back();
    }
    InputQueue.push_front(prod.leftSymbol);
}

}  // namespace gram