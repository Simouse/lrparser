#include "src/parser/LRParser.h"

#include <cassert>
#include <memory>
#include <stack>
#include <stdexcept>
#include <string>
#include <utility>
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
// static String createStateName(const gram::Grammar &gram, const gram::Symbol
// &N,
//                               const std::vector<SymbolID> &productionBody,
//                               size_t dotPos) {
//     assert(dotPos <= productionBody.size());

//     const auto &symbols = gram.getAllSymbols();
//     String s = N.name;
//     s += " ->";

//     size_t j = 0;
//     for (; j < dotPos; ++j) {
//         s += ' ';
//         s += symbols[productionBody[j]].name;
//     }
//     s += ' ';
//     s += Grammar::SignStrings::dot;
//     for (; j < productionBody.size(); ++j) {
//         s += ' ';
//         s += symbols[productionBody[j]].name;
//     }
//     return s;
// }

// S -> aB : S0 = {S -> .aB, S -> a.B, S -> aB.}
void LRParser::buildNFAStateLabels() {
    const auto &productionTable = gram.getProductionTable();
    const auto &symbols = gram.getAllSymbols();
    for (auto const &production : productionTable) {
        std::vector<const char *> vec;
        const auto &rhs = production.rightSymbols;
        // i: position of dot
        for (size_t i = 0; i <= rhs.size(); ++i) {
            String s = symbols[production.leftSymbol].name;
            s += " ->";
            size_t j = 0;
            for (; j < i; ++j) {
                s += ' ';
                s += symbols[rhs[j]].name;
            }
            s += ' ';
            s += Grammar::SignStrings::dot;
            for (; j < rhs.size(); ++j) {
                s += ' ';
                s += symbols[rhs[j]].name;
            }
            vec.push_back(newString(s));
        }
        labelMap.push_back(std::move(vec));
    }
}

// `rhsIndex`: points to the index of non-terminal symbol which we want to
// resolve local follow constraints
static auto
resolveLocalConstraints(Grammar const &gram,
                        util::BitSet<gram::ActionID> const &parentConstraints,
                        Production const &production, size_t rhsIndex)
    -> util::BitSet<gram::ActionID> {

    auto const &symbols = gram.getAllSymbols();
    auto const &rhs = production.rightSymbols;
    util::BitSet<gram::ActionID> constraints(symbols.size());

    bool allNullable = true;
    for (size_t i = rhsIndex + 1; allNullable && i < rhs.size(); ++i) {
        if (!symbols[rhs[i]].nullable.value())
            allNullable = false;
        constraints |= symbols[rhs[i]].firstSet;
    }
    if (allNullable)
        constraints |= parentConstraints;

    return constraints;
}

void LRParser::buildNFA() {
    Automaton &M = this->nfa;
    const Grammar &G = this->gram;
    const auto &symbols = G.getAllSymbols();
    auto const &productionTable = G.getProductionTable();
    util::Formatter f;
    constexpr const char *outFilePrefix = "build_nfa";

    buildNFAStateLabels();

    nfa.setDumpFlag(shouldIncludeConstraintsInDump());

    auto estimatedStateCount = symbols.size() + productionTable.size() * 4;

    // Store seeds and check if a seed exists. See definition of StateSeed.
    std::unordered_map<StateSeed, std::vector<StateID>,
                       decltype(getSeedHashFunc()),
                       decltype(getSeedEqualFunc())>
        seeds(estimatedStateCount, getSeedHashFunc(), getSeedEqualFunc());

    // One state can only link to one seed, because if constraints are
    // different, LR1 should already mark sources as different states, and LR0
    // will merge them so no new state was generated.
    // { State ID => iterator to `seeds` }
    std::unordered_map<StateID, decltype(seeds.begin())> epsilonLinks;

    // Mark symbols that have their productions visited
    std::stack<decltype(seeds.begin())> unvisitedSeeds;

    auto addNewDependency = [&epsilonLinks, &unvisitedSeeds,
                             &seeds](StateID state, SymbolID symbolID,
                                     util::BitSet<ActionID> *constraints) {
        StateSeed seed = std::make_pair(symbolID, constraints);
        // Find iterator
        auto result = seeds.try_emplace(seed, std::vector<StateID>());
        // Add a dependency link
        epsilonLinks.try_emplace(state, result.first);
        // Add the seed to stack if it's new
        if (result.second) {
            unvisitedSeeds.emplace(result.first);
        }
    };

    // First we need to copy all actions by order, so symbols and actions are
    // paired.
    for (auto const &symbol : symbols) {
        auto actionID = M.addAction(newString(symbol.name));
        if (symbol.id == gram.getEpsilonSymbol().id) {
            M.setEpsilonAction(actionID);
        }
    }

    // Create S' (start symbol in augmented grammar) for S
    {
        auto constraints = newConstraint(symbols.size());
        constraints->insert(static_cast<ActionID>(G.getEndOfInputSymbol().id));

        auto const &dotSign = Grammar::SignStrings::dot;
        auto &start = G.getStartSymbol();
        String name = start.name + '\'';
        StateID s0 = M.addState(
            newString(name + " -> " + dotSign + " " + start.name), constraints);
        StateID s1 = M.addState(
            newString(name + " -> " + start.name + " " + dotSign), constraints);
        M.addTransition(s0, s1, static_cast<ActionID>(start.id));
        M.markStartState(s0);
        M.markFinalState(s1);
        this->extendedStart = s0;
        this->extendedEnd = s1;

        addNewDependency(s0, start.id, constraints);
    }

    // Process all seeds
    while (!unvisitedSeeds.empty()) {
        auto seedIter = unvisitedSeeds.top();
        unvisitedSeeds.pop();
        for (auto productionID : symbols[seedIter->first.first].productions) {
            auto const &production = productionTable[productionID];
            auto const &rhs = production.rightSymbols;
            StateID firstStateID;
            // Add states
            for (size_t i = 0; i <= rhs.size(); ++i) {
                const char *name = labelMap[productionID][i];
                StateID stateID = M.addState(name, seedIter->first.second);
                if (i == 0)
                    firstStateID = stateID;
            }
            // Link these states
            for (size_t i = 0; i < rhs.size(); ++i) {
                auto s1 = static_cast<StateID>(firstStateID + i);
                auto s2 = static_cast<StateID>(firstStateID + i + 1);
                auto const &curSymbol = symbols[rhs[i]];
                M.addTransition(s1, s2, static_cast<ActionID>(curSymbol.id));
                // Add new dependency
                if (curSymbol.type == SymbolType::NON_TERM) {
                    assert(seedIter->first.second);
                    auto constraints = resolveLocalConstraints(
                        gram, *seedIter->first.second, production, i);
                    addNewDependency(s1, curSymbol.id,
                                     newConstraint(std::move(constraints)));
                }
            }

            auto lastStateID = static_cast<StateID>(firstStateID + rhs.size());
            // Mark possible final state
            if (canMarkFinal(seedIter->first, production)) {
                M.markFinalState(lastStateID);
            }
            // Record last state of a production
            // (Production ID is needed in parser table)
            reduceMap[lastStateID] = productionID;
            // We are in a loop. So if a symbol has multiple productions,
            // `seed->second` will store multiple start states.
            seedIter->second.push_back(firstStateID);
        }
    }

    // display(AUTOMATON, INFO, "Mark final states", &M, (void *)outFilePrefix);

    // Creating epsilon edges for each symbol
    for (const auto &link : epsilonLinks) {
        StateID from = link.first;
        for (StateID to : link.second->second) {
            M.addEpsilonTransition(from, to);
        }
    }

    display(AUTOMATON, INFO, "NFA is built", &M, (void *)outFilePrefix);
}

void LRParser::buildDFA() {
    // Dump flag is inherited, no need to set again
    nfa.setClosureEqualFunc(getClosureEqualFunc());
    nfa.setDuplicateClosureHandler(getDuplicateClosureHandler());
    dfa = nfa.toDFA();
    display(AUTOMATON, INFO, "DFA is built", &dfa, (void *)"build_dfa");
}

void LRParser::buildParserTable() {
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
        auto const &trans = *state.transitions;
        for (auto const &tran : trans) {
            ParseAction item{(symbols[tran.action].type == SymbolType::NON_TERM)
                                 ? ParseAction::GOTO
                                 : ParseAction::SHIFT,
                             tran.to};
            addParserTableEntry(state.id, tran.action, item);
        }
    }

    // Process Reduce items
    util::BitSet<StateID> reducibleStateMask{formerStates.size()};
    for (auto const &entry : reduceMap) {
        reducibleStateMask.insert(entry.first);
    }

    for (int i = 0; i < nDFAStates; ++i) {
        auto state = static_cast<StateID>(i);
        auto closure = closures[state]; // Copy a bitmap
        closure &= reducibleStateMask;
        for (auto substate : closure) {
            for (int j = 0; j < nActions; ++j) {
                auto action = static_cast<ActionID>(j);
                if (symbols[action].type == SymbolType::NON_TERM) {
                    continue;
                }
                ProductionID prodID = reduceMap.at(substate);
                addParserTableEntry(state, action,
                                    ParseAction{ParseAction::REDUCE, prodID},
                                    substate);
            }
        }
    }

    // Process end of input
    auto endOfInput = static_cast<ActionID>(gram.getEndOfInputSymbol().id);
    for (int i = 0; i < nDFAStates; ++i) {
        if (closures[i].contains(this->extendedEnd)) {
            addParserTableEntry(StateID{i}, endOfInput,
                                ParseAction{ParseAction::SUCCESS, -1});
        }
    }

    display(PARSE_TABLE, INFO, "Parse table", this);
}

auto LRParser::getClosureEqualFunc() const -> Automaton::ClosureEqualFuncType {
    return [](Closure const &a, Closure const &b) { return a == b; };
}

auto LRParser::getDuplicateClosureHandler() const
    -> Automaton::DuplicateClosureHandlerType {
    return [](Closure const &a,
              Closure const &b) { /* Default: do nothing */ };
}

void LRParser::addParserTableEntry(StateID state, ActionID act,
                                   ParseAction pact, StateID subState) {
    using std::set;
    using std::vector;
    if (!canAddParserTableEntry(state, act, pact, subState)) {
        return;
    }
    if (parserTable.empty()) {
        parserTable = vector<vector<set<ParseAction>>>(
            dfa.getAllStates().size(),
            vector<set<ParseAction>>(gram.getAllSymbols().size()));
    }
    parserTable[state][act].insert(pact);
}

String LRParser::dumpParserTableEntry(StateID state, ActionID action) const {
    auto const &items = parserTable.at(state).at(action);
    String s;
    bool commaFlag = false;
    for (auto const &item : items) {
        if (commaFlag)
            s += ',';
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
                "dumpParserTableEntry(): Unknown parse action type");
        }
    }
    return s;
}

void LRParser::readSymbol(util::TokenReader &reader) {
    String s;
    if (reader.getToken(s)) {
        auto const &symbol = gram.findSymbol(s);
        if (!launchArgs.allowNonterminalAsInputs &&
            symbol.type == SymbolType::NON_TERM) {
            throw std::runtime_error("Non-terminals as inputs are not allowed");
        }
        if (symbol.id == gram.getEpsilonSymbol().id) {
            throw std::runtime_error("Epsilon cannot be used in input");
        } else if (symbol.id == gram.getEndOfInputSymbol().id) {
            inputFlag = false;
        }
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
            parserTable[stateStack.back()][InputQueue.front()];

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
        auto const &decision = *tableEntry.begin();
        switch (decision.type) {
        case ParseAction::GOTO:
        case ParseAction::SHIFT:
            stateStack.push_back(decision.dest);
            symbolStack.push_back(InputQueue.front());
            InputQueue.pop_front();
            display(LOG, VERBOSE,
                    decision.type == ParseAction::GOTO ? "Apply GOTO rule"
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

} // namespace gram