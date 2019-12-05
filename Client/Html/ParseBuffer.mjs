export default class ParseBuffer
{
    constructor(buffer) {
        this.buffer = buffer;
        this._pos = 0;
    }

    get eos() {
        return this.pos >= this.length;
    }
    get pos() {
        return this._pos;
    }
    get length() {
        return this.buffer.length;
    }
    get tailLength() {
        return this.buffer.length - this.pos;
    }
    get currentChar() {
        return this.charAt(this.pos);
    }
    get currentCharCode() {
        return this.charCodeAt(this.pos);
    }
    get tail() {
        return this.buffer.substring(this.pos, this.pos + this.tailLength);
    }

    clone() {
        let clone = new ParseBuffer(this.buffer);
        clone._pos = this.pos;

        return clone;
    }
    assign(parseBuffer) {
        this.buffer = parseBuffer.buffer;
        this._pos = parseBuffer.pos;
    }

    advance(count) {
        if(count === undefined)
            ++this._pos
        else
            this._pos += count;
    }
    charAt(index) {
        return this.buffer.charAt(index);
    }
    charCodeAt(index) {
        return this.buffer.charCodeAt(index);
    }
    startsWith(searchString) {
        return this.buffer.startsWith(searchString, this.pos);
    }
}
