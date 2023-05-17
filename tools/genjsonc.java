///usr/bin/env jbang "$0" "$@" ; exit $?


import static java.lang.System.out;

public class genjsonc {

    public static final String[] attributes = new String[]{
            "classes_jni",
            "classes_reachable",
            "classes_reflection",
            "classes_total",
            "code_area_bytes",
            "fields_jni",
            "fields_reachable",
            "fields_reflection",
            "fields_total",
            "gc_total_ms",
            "image_heap_bytes",
            "image_total_bytes",
            "methods_jni",
            "methods_reachable",
            "methods_reflection",
            "methods_total",
            "peak_rss_bytes",
            "resources_bytes",
            "resources_count",
            "total_build_time_ms"};
    public static final String template = """
            else if (strncmp("%s", attribute_name, strlen(attribute_name)) == 0) {
                for (int j = 0; j < array_length; j++) {
                    buildTime.%s[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                    if (buildTime.%s[j] > buildTimeLabels.max_%s) {
                        buildTimeLabels.max_%s = buildTime.%s[j];
                    }
                }
            }
                        """;
    public static void main(String... args) {
        for (String a: attributes) {
            out.printf(template,a,a,a,a,a,a);
        }
    }
}
