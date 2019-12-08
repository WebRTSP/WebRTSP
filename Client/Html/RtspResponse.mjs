export default class Response
{
    constructor() {
        this.protocol = null;
        this.statusCode = null;
        this.reasonPhrase = null;
        this.cseq = null;

        this.headerFields = new Map();

        this.body = null;
    }
}
