export default class Request
{
    constructor() {
        this.method = null;
        this.uri = null;
        this.protocol = null;
        this.cseq = null;

        this.headerFields = new Map();

        this.body = null;
    }
}
