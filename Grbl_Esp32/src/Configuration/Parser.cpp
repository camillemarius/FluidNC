/*
    Part of Grbl_ESP32
    2021 -  Stefan de Bruijn

    Grbl_ESP32 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Grbl_ESP32 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Grbl_ESP32.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Parser.h"

#include "ParseException.h"
#include "../EnumItem.h"

#include "../Logging.h"

#include <climits>

namespace Configuration {
    Parser::Parser(const char* start, const char* end) : Tokenizer(start, end), current_() {
        Tokenize();
        current_ = token_;
        if (current_.kind_ != TokenKind::Eof) {
            Tokenize();
        }
    }

    void Parser::parseError(const char* description) const {
        // Attempt to use the correct position in the parser:
        if (current_.keyEnd_) {
            throw ParseException(start_, current_.keyEnd_, description);
        } else {
            Tokenizer::ParseError(description);
        }
    }

    bool Parser::is(const char* expected) const {
        if (current_.keyStart_ == nullptr) {
            return false;
        }
        auto len = strlen(expected);
        if (len != (current_.keyEnd_ - current_.keyStart_)) {
            return false;
        }
        return !strncmp(expected, current_.keyStart_, len);
    }

    /// <summary>
    /// MoveNext: moves to the next entry in the current section. By default we're in the
    /// root section.
    /// </summary>
    bool Parser::moveNext() {
        // While the indent of the token is > current indent, we have to skip it. This is a
        // sub-section, that we're apparently not interested in.
        while (token_.indent_ > current_.indent_) {
            Tokenize();
        }

        // If the indent is the same, we're in the same section. Update current, move to next
        // token.
        if (token_.indent_ == current_.indent_) {
            current_ = token_;
            Tokenize();
        } else {
            // Apparently token_.indent < current_.indent_, which means we have no more items
            // in our tokenizer that are relevant.
            //
            // Note that we want to preserve current_.indent_!
            current_.kind_ = TokenKind::Eof;
        }

        return current_.kind_ != TokenKind::Eof;
    }

    void Parser::enter() {
        indentStack_.push(current_.indent_);

        // If we can enter, token_.indent_ > current_.indent_:
        if (token_.indent_ > current_.indent_) {
            current_ = token_;
            Tokenize();
        } else {
            current_         = TokenData();
            current_.indent_ = INT_MAX;
        }
        indent_ = current_.indent_;
    }

    void Parser::leave() {
        // While the indent of the tokenizer is >= current, we can ignore the contents:
        while (token_.indent_ >= current_.indent_ && token_.kind_ != TokenKind::Eof) {
            Tokenize();
        }

        // At this point, we just know the indent is smaller. We don't know if we're in
        // the *right* section tho.
        auto last = indentStack_.top();
        indent_   = last;
        indentStack_.pop();

        if (last == token_.indent_) {
            // Yes, the token continues where we left off:
            current_ = token_;
            Tokenize();
        } else {
            current_         = TokenData();
            current_.indent_ = last;
        }
    }

    StringRange Parser::stringValue() const { return StringRange(current_.sValueStart_, current_.sValueEnd_); }

    bool Parser::boolValue() const {
        auto str = StringRange(current_.sValueStart_, current_.sValueEnd_);
        return str.equals("true");
    }

    int Parser::intValue() const {
        auto    str = StringRange(current_.sValueStart_, current_.sValueEnd_);
        int32_t value;
        if (!str.isInteger(value)) {
            parseError("Expected an integer value like 123");
        }
        return value;
    }

    float Parser::floatValue() const {
        auto  str = StringRange(current_.sValueStart_, current_.sValueEnd_);
        float value;
        if (!str.isFloat(value)) {
            parseError("Expected a float value like 123.456");
        }
        return value;
    }

    Pin Parser::pinValue() const {
        auto str = StringRange(current_.sValueStart_, current_.sValueEnd_);
        return Pin::create(str);
    }

    IPAddress Parser::ipValue() const {
        IPAddress ip;
        auto      str = StringRange(current_.sValueStart_, current_.sValueEnd_);
        if (!ip.fromString(str.str())) {
            parseError("Expected an IP address like 192.168.0.100");
        }
        return ip;
    }

    int Parser::enumValue(EnumItem* e) const {
        auto str = StringRange(current_.sValueStart_, current_.sValueEnd_);
        for (; e->name; ++e) {
            if (str.equals(e->name)) {
                break;
            }
        }
        return e->value;
    }
}
