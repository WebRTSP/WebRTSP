export default class Token
{
    constructor(parseBuffer) {
        this.buffer = parseBuffer.buffer;
        this.pos = parseBuffer.pos;
        this.length = 0;
    }

    get string() {
        return this.buffer.substr(this.pos, this.length);
    }
    get empty() {
        if(!this.buffer || !this.length)
            return true;

        return this.pos >= this.buffer.length;
    }

    charAt(index) {
        return this.buffer.charAt(this.pos + index);
    }
    charCodeAt(index) {
        return this.buffer.charCodeAt(this.pos + index);
    }
    startsWith(searchString) {
        return this.buffer.startsWith(searchString, this.pos);
    }
}
