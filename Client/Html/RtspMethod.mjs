export const OPTIONS = 1;
export const DESCRIBE = 2;
// export const ANNOUNCE = 3;
export const SETUP = 4;
export const PLAY = 5;
// export const PAUSE = 6;
export const TEARDOWN = 7;
// export const GET_PARAMETER = 8;
// export const SET_PARAMETER = 9;
// export const REDIRECT = 10;
// export const RECORD = 11;

const names =
{
    OPTIONS,
    DESCRIBE,
    // ANNOUNCE,
    SETUP,
    PLAY,
    // PAUSE,
    TEARDOWN,
    // GET_PARAMETER,
    // SET_PARAMETER,
    // REDIRECT,
    // RECORD,
}

export function Name(method)
{
    for(let key in names) {
        if(names[key] === method)
            return key;
    }

    return undefined;
}

export function Parse(token)
{
    if(token.empty)
        return undefined;

    for(let key in names) {
        if(key.length == token.length &&
           token.startsWith(key))
        {
            return names[key];
        }
    }

    return undefined;
}
