$(document).ready(function() {
    var page = new Vue({
        el: "#page-content",
        data: {
            tvs: [],
        },
        methods: {
            modify: function(event) {
                var mk = $(event.currentTarget).attr("mk");
                console.debug(mk);

            },
        },
    });

    var reload_data = function() {
        page.tvs = [];
        __request("list", {}, function(res) {
            console.debug(res);
        });
    }
    reload_data();
});

